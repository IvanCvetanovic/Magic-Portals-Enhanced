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
# VERSION is the program's number, the ONE definition of it: the Windows release's version information carries it (game/windows). 0.1.0 is a
# pre-release's placeholder, not a decision: what the first release is called is the owner's.
project(MagicPortals VERSION 0.1.0 LANGUAGES C CXX)

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

# ---- What kind of program this is (planning step 201, PORT-3) ---------------------------------------------------------------------------
# Four options, and none of them changes a developer's desktop build: with every one at its default build/ is the program it was, picture for
# picture. They are settled here, above every target, so game/ and tests/ read one answer. docs/packaging.md says which belong in a release.
#
# MAGICPORTALS_MOBILE: a phone's build. ON by itself when the target is Android or iOS, OFF otherwise. It makes the run start under the phone
# profile (game/data/engine.json "mobile": a lowest Vulkan version of 1.1 among it) unless --profile says otherwise, and takes the developer
# tools and the measurement tool out of the program. It adds no entry point: an Android or iOS shell is a later step. It may be turned ON on a
# desktop to build what a phone's build compiles, which is how the compilers are asked about it without a phone.
set(_magicPortalsMobileDefault OFF)
if (ANDROID OR IOS)
    set(_magicPortalsMobileDefault ON)
endif()
option(MAGICPORTALS_MOBILE "A phone's build: the phone profile by default, no developer tools (ON by itself for an Android or iOS target)" ${_magicPortalsMobileDefault})

# MAGICPORTALS_DEV_TOOLS: the developer's command-line flags (--route, --tap, --hold, --visit-levels, --place, --burn-dragon, the measurement
# switches ...) and the two files behind them (game/LevelVisit.cpp, game/Route.cpp). A player needs none of them and a phone cannot type them. AUTO,
# the default, is ON, except that MAGICPORTALS_MOBILE turns it OFF; ON or OFF says it outright and wins either way. (Three states and not a plain
# option, because a plain option's default is written into the cache once: a tree first configured without MAGICPORTALS_MOBILE would keep ON when
# it was later configured with it.) The suites always build with the tools: test_mp_routes needs Route.cpp.
# Exactly these three words, in any case, and nothing else (planning step 202, PORT3-1): the option exists to take the developer's flags out of a
# player's program, so a typo (OF, BANANA, an empty value) must stop the configure and not be read as CMake's truth test of a variable, which
# calls every word that is not a false constant ON. The old spellings of false (0, NO, FALSE) and of true (1, TRUE, YES) are refused too: no
# script of this repository passes one (tools/build_linux.sh checks its own argument), and a rule with two spellings of a word is the rule that failed.
set(MAGICPORTALS_DEV_TOOLS AUTO CACHE STRING "The developer's flags and tools: AUTO (ON; OFF with MAGICPORTALS_MOBILE), ON or OFF")
set_property(CACHE MAGICPORTALS_DEV_TOOLS PROPERTY STRINGS AUTO ON OFF)
string(TOUPPER "${MAGICPORTALS_DEV_TOOLS}" _magicPortalsDevTools)
if (_magicPortalsDevTools STREQUAL "AUTO")
    if (MAGICPORTALS_MOBILE)
        set(MAGICPORTALS_DEV_TOOLS_ENABLED OFF)
    else()
        set(MAGICPORTALS_DEV_TOOLS_ENABLED ON)
    endif()
elseif (_magicPortalsDevTools STREQUAL "ON")
    set(MAGICPORTALS_DEV_TOOLS_ENABLED ON)
elseif (_magicPortalsDevTools STREQUAL "OFF")
    set(MAGICPORTALS_DEV_TOOLS_ENABLED OFF)
else()
    message(FATAL_ERROR "MAGICPORTALS_DEV_TOOLS is \"${MAGICPORTALS_DEV_TOOLS}\", which is not one of its three values: AUTO (ON, and OFF with "
                        "MAGICPORTALS_MOBILE), ON or OFF (in any case). Nothing was configured: a mistyped value would otherwise ship, or "
                        "leave out, the developer's flags without a word. Pass -DMAGICPORTALS_DEV_TOOLS=OFF for a player's program.")
endif()

# MAGICPORTALS_WIN_GUI: the Windows RELEASE executable (game/windows/MagicPortalsWindows.cmake): no console window, vulkan-1.dll loaded late behind a
# check that says in words what is missing, and version information. OFF by default and in build/, because a program with no console gives a
# script no stdout, no stderr and no --help: the parity rig and every script that launches the game need the console program. Use a build folder
# of its own for it. MSVC only (a configure with it ON on anything else stops).
option(MAGICPORTALS_WIN_GUI "Windows release program: GUI subsystem, vulkan-1.dll delay-loaded, version information (MSVC)" OFF)
# MAGICPORTALS_ICON: the .ico the release program shows. Never part of this repository (the hard rule: no image file is committed, an icon
# included): tools/art/make_app_icon.py draws one into out/dist/icons, and this names it. Empty, the program has no icon. Used only with
# MAGICPORTALS_WIN_GUI.
set(MAGICPORTALS_ICON "" CACHE FILEPATH "An .ico file for the Windows release program (empty: none); see tools/art/make_app_icon.py")
if (MAGICPORTALS_WIN_GUI)
    if (NOT (WIN32 AND MSVC))
        message(FATAL_ERROR "MAGICPORTALS_WIN_GUI is the Windows release build and is made with MSVC; this configure is not one. Leave it OFF here.")
    endif()
    # The resource compiler is a language of its own to CMake; asked for here, above every target, rather than left to the platform's defaults.
    enable_language(RC)
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
