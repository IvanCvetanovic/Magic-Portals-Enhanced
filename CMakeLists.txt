# Magic Portals on the Supersonic Engine.
#
#   engine/   the engine, a git submodule pinned to a commit. Added as a
#             subproject, it builds SupersonicCore and the test harness and
#             nothing else: no editor, no script plugin, none of its suites.
#   game/     the port: sim/ (MagicPortalsSim), the layer (MagicPortalsGame),
#             the executable and the level30 measurement tool.
#   tests/    the port's suites, test_mp_*.
#
# What the port reads at run time is this repository's own: data/ (Enhanced's
# tables), game/data/ (the port's), and the gitignored out/ and reference/ that
# the converter and the original's APK fill. game/sim/CMakeLists.txt has the
# paths, each a cache variable that one -D moves.
#
# Configure it as the engine's own build is configured (tools/parity/port_build.bat
# does exactly this):
#   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DSUPERSONIC_ENABLE_VALIDATION=ON

cmake_minimum_required(VERSION 3.20)
project(MagicPortals LANGUAGES C CXX)

# The engine's headers are C++20, and SupersonicCore says so on its interface;
# the port's own code is held to the same standard here, where the engine's
# directory settings do not reach.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/engine/CMakeLists.txt")
    message(FATAL_ERROR
        "engine/ is empty: the Supersonic Engine is a git submodule. Run\n"
        "  git submodule update --init\n"
        "(or clone with --recurse-submodules) and configure again.")
endif()
add_subdirectory(engine)

# The executables. The port's libraries build without them, because the suites
# link those; this repository is the game, so they are on by default.
option(SUPERSONIC_BUILD_MAGICPORTALS "Build MagicPortals and the MagicPortalsSpike measurement tool" ON)
add_subdirectory(game)

option(MAGICPORTALS_BUILD_TESTS "Build the port's test_mp_* suites" ON)
if (MAGICPORTALS_BUILD_TESTS)
    # The engine enables testing only for its own suites, which are off here.
    enable_testing()
    add_subdirectory(tests)
endif()
