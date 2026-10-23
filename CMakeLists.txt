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

# Whether the build bakes THIS machine's absolute data paths into the program (planning step 200, PORT-2). ON is a developer's build and
# is what every build here has been: the program finds the repository's out/, data/ and reference/ with no word from anyone, and the
# suites read them the same way. OFF is a release build: the six MAGICPORTALS_* paths compile to empty strings, so the program carries no
# account name and no checkout path, and finds its data at run time from --root, a folder next to the program, or the user's data folder
# (game/sim/AssetRoot.hpp, docs/packaging.md). A run that finds none says so and exits 1. The suites are a developer's tool and read the
# baked paths, so they are not built by default when this is OFF. Use a build folder of its own: the cache of the other kind keeps its
# paths.
option(MAGICPORTALS_BAKED_PATHS "Bake this machine's data paths into the build (ON: a developer's build; OFF: a release build that finds its data at run time)" ON)
if (NOT MAGICPORTALS_BAKED_PATHS)
    # The engine bakes the absolute path of its own checkout as the last place to look for its shaders, unless the variable is already
    # defined (engine/CMakeLists.txt, SUPERSONIC_ASSET_ROOT). A release finds its shaders beside the program or in the working directory,
    # so none is baked. A normal variable, not a cache entry: it hides one, and a later ON configure of the same folder is undisturbed.
    set(SUPERSONIC_ASSET_ROOT "")
    # And the compiler puts the path of every source file into what it builds (assert and log text, __FILE__): map the checkout and the
    # build folder to relative names, so no account name is left in the program. For every target, the engine's included.
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options("-ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=." "-ffile-prefix-map=${CMAKE_BINARY_DIR}=.build")
    endif()
endif()

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

# Off in a release configure (MAGICPORTALS_BAKED_PATHS=OFF): the suites read the baked paths.
option(MAGICPORTALS_BUILD_TESTS "Build the port's test_mp_* suites" ${MAGICPORTALS_BAKED_PATHS})
if (MAGICPORTALS_BUILD_TESTS)
    # The engine enables testing only for its own suites, which are off here.
    enable_testing()
    add_subdirectory(tests)
endif()
