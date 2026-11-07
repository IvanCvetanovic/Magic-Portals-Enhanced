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
project(MagicPortals VERSION 0.1.6 LANGUAGES C CXX)

# Objective-C++ on Apple platforms only (planning step 206, PORT-8; the sibling port's top-level file, whose Apple build ran on GitHub's macOS runners, is the
# model): the entries in game/macos and game/ios ask the system for its folders and the engine's iOS backend is UIKit. Objective-C as well, for GLFW's Cocoa
# backend (.m), which would otherwise be compiled as Objective-C++. Enabled here, the directory above everything that compiles or links them (the engine
# enables them too, for its own build). NOTHING in this block is reached when APPLE is false: the generated build files of Windows and Linux are the same.
if (APPLE)
    enable_language(OBJC OBJCXX)

    # Apple's linker (Xcode 15 and later) warns about every library named twice on a link line, which CMake writes whenever a target links SupersonicCore
    # both itself and through MagicPortalsGame, as the suites do: a warning per target, about nothing. Silenced where the linker has the option.
    include(CheckLinkerFlag)
    check_linker_flag(CXX "LINKER:-no_warn_duplicate_libraries" MAGICPORTALS_LD_NO_WARN_DUPLICATES)
    if (MAGICPORTALS_LD_NO_WARN_DUPLICATES)
        add_link_options("LINKER:-no_warn_duplicate_libraries")
    endif()
endif()

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
# An Android program is a release build by nature (planning step 203, PORT-5): there is no repository on a phone, so the default there is OFF, and a
# configure that asks for ON is refused below rather than baking an account name and a checkout path into a program for a phone. An iOS program is the
# same (planning step 206, PORT-8).
set(_magicPortalsBakedDefault ON)
if (ANDROID OR IOS)
    set(_magicPortalsBakedDefault OFF)
endif()
option(MAGICPORTALS_BAKED_PATHS "Bake this machine's data paths into the build (ON: a developer's build; OFF: a release build that finds its data at run time)" ${_magicPortalsBakedDefault})
if (ANDROID AND MAGICPORTALS_BAKED_PATHS)
    message(FATAL_ERROR "MAGICPORTALS_BAKED_PATHS is ON for an Android configure. There is no repository on a phone: the program finds its data in the folder the app unpacks it to "
                        "(game/android/AndroidMain.cpp). Leave it OFF (tools/build_android.sh does).")
endif()
if (IOS AND MAGICPORTALS_BAKED_PATHS)
    message(FATAL_ERROR "MAGICPORTALS_BAKED_PATHS is ON for an iOS configure. There is no repository on a phone: the program finds its data in the app bundle "
                        "(game/ios/IOSMain.mm). Leave it OFF, which is its default there.")
endif()
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
# tools and the measurement tool out of the program. Its Android entry point is game/android/AndroidMain.cpp (planning step 203) and its iOS one
# game/ios/IOSMain.mm (planning step 206, written and never run). It may be turned ON on a desktop to build what a phone's build compiles, which is how the
# compilers are asked about it without a phone.
set(_magicPortalsMobileDefault OFF)
if (ANDROID OR IOS)
    set(_magicPortalsMobileDefault ON)
endif()
option(MAGICPORTALS_MOBILE "A phone's build: the phone profile by default, no developer tools (ON by itself for an Android or iOS target)" ${_magicPortalsMobileDefault})
# The Android program is entered by game/android/AndroidMain.cpp and starts under the phone profile (Vulkan 1.1): without it the engine's default (1.2) refuses
# many phones' stock drivers. Turning it OFF on an Android configure would build a program that cannot start on them, so it stops here (planning step 203).
if (ANDROID AND NOT MAGICPORTALS_MOBILE)
    message(FATAL_ERROR "MAGICPORTALS_MOBILE is OFF for an Android configure. The Android program is a phone's build (the phone profile, Vulkan 1.1); leave it ON, which is its default there.")
endif()

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

# MAGICPORTALS_PERF_DIAG (planning step 205, PORT-7): a PERFORMANCE-DIAGNOSTIC build, OFF by default and in every release. When ON the engine's frame readout is over the
# picture and its statistics line goes into the game's own log every 5 seconds (GameManifest::perfOverlay and perfLogSeconds), for a tester's phone to say where its
# frames go; nothing else changes (not the profile, not the sample cap, not the developer's flags). It is independent of MAGICPORTALS_MOBILE and of MAGICPORTALS_DEV_TOOLS:
# a tester's build must not carry the developer's flags. When OFF none of it is compiled in. Like the other two it reaches every source of the MagicPortals program as a
# definition of that target (game/CMakeLists.txt) and BuildProfile.hpp stops a translation unit that lacks it.
option(MAGICPORTALS_PERF_DIAG "A performance-diagnostic build: the engine's frame readout and a statistics line in the log every 5 s (OFF in every release)" OFF)

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

# Off in a release configure (MAGICPORTALS_BAKED_PATHS=OFF): the suites read the baked paths. Never for Android or iOS: they are executables that read the
# repository, and a phone has neither (planning steps 203 and 206).
option(MAGICPORTALS_BUILD_TESTS "Build the port's test_mp_* suites" ${MAGICPORTALS_BAKED_PATHS})
if (ANDROID AND MAGICPORTALS_BUILD_TESTS)
    message(FATAL_ERROR "MAGICPORTALS_BUILD_TESTS is ON for an Android configure. The suites are not built for Android: they run on the machine that builds them.")
endif()
if (IOS AND MAGICPORTALS_BUILD_TESTS)
    message(FATAL_ERROR "MAGICPORTALS_BUILD_TESTS is ON for an iOS configure. The suites are not built for iOS: an app cannot start the process a suite is, and they run on the machine that builds them.")
endif()
if (MAGICPORTALS_BUILD_TESTS)
    # The engine enables testing only for its own suites, which are off here.
    enable_testing()
    add_subdirectory(tests)
endif()
