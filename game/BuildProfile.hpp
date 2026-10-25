#pragma once

// WHAT KIND OF BUILD THIS IS, as two compile-time constants (planning step 201, PORT-3; made safe for a second translation unit by step 202).
//
//   kMobileBuild  a phone's build (an Android or iOS target, or MAGICPORTALS_MOBILE=ON): the run starts under the phone
//                 profile, game/data/engine.json's "mobile" block, unless --profile says otherwise.
//   kDevTools     the developer's command-line flags (--route, --tap, --visit-levels ...) and the code behind them are compiled in.
//                 MAGICPORTALS_DEV_TOOLS=OFF, which a mobile build defaults to, leaves a player's build without them.
//   kPerfDiag     a PERFORMANCE-DIAGNOSTIC build (planning step 205): the engine's frame readout is over the picture and its statistics are in the game's log every 5
//                 seconds, for a tester's phone to say where its frames go. MAGICPORTALS_PERF_DIAG=OFF (every default and every release build) has none of it. It is
//                 independent of the other two: a tester's build must not carry the developer's flags. The macro is the way the code is left out (a log string that
//                 is not compiled in is not in the program), the constant is for code that wants a value.
//
// WHERE THE FACTS COME FROM. CMake (game/CMakeLists.txt) hands both to EVERY source file of the MagicPortals program as definitions of that
// target, so main.cpp and any file added to the program later agree. A translation unit that is not part of the program (a suite) has no
// build to describe, and says so outright by defining MAGICPORTALS_BUILD_PROFILE_DESKTOP before it includes this header: it then sees what a
// developer's desktop build is, no phone and the developer's tools in, which is the build every suite is (test_mp_routes needs Route.cpp).
//
// WHAT IT NO LONGER DOES is guess. It used to default a missing fact to the desktop's, so a translation unit of a MOBILE build that was not
// given the definitions would have compiled, quietly disagreed with main.cpp, and made `kMobileBuild` mean two things in one program (an ODR
// violation, and a wrong answer). A translation unit that is given neither the facts nor the declaration now stops the build with a message.
//
// A macro as well as a constant, because the dev tools (and the diagnostics) are not a value to branch on: with them off, main.cpp does not name the types of
// LevelVisit.cpp and Route.cpp at all, and those two files are not part of the program.

#if defined(MAGICPORTALS_BUILD_PROFILE_DESKTOP)
#if defined(MAGICPORTALS_MOBILE) || defined(MAGICPORTALS_DEV_TOOLS) || defined(MAGICPORTALS_PERF_DIAG)
#error "BuildProfile.hpp: this translation unit says it is a developer's desktop one (MAGICPORTALS_BUILD_PROFILE_DESKTOP) and was also given the build's facts (MAGICPORTALS_MOBILE, MAGICPORTALS_DEV_TOOLS or MAGICPORTALS_PERF_DIAG): it is one or the other"
#endif
#define MAGICPORTALS_MOBILE 0
#define MAGICPORTALS_DEV_TOOLS 1
#define MAGICPORTALS_PERF_DIAG 0
#elif !defined(MAGICPORTALS_MOBILE) || !defined(MAGICPORTALS_DEV_TOOLS) || !defined(MAGICPORTALS_PERF_DIAG)
#error "BuildProfile.hpp: this translation unit was given MAGICPORTALS_MOBILE, MAGICPORTALS_DEV_TOOLS and MAGICPORTALS_PERF_DIAG neither as all three (game/CMakeLists.txt gives them to every source of the MagicPortals program) nor as MAGICPORTALS_BUILD_PROFILE_DESKTOP (a suite's own declaration). Give it one, so that it cannot disagree with main.cpp."
#endif

namespace MagicPortals {

inline constexpr bool kMobileBuild = MAGICPORTALS_MOBILE != 0;
inline constexpr bool kDevTools = MAGICPORTALS_DEV_TOOLS != 0;
inline constexpr bool kPerfDiag = MAGICPORTALS_PERF_DIAG != 0;

} // namespace MagicPortals
