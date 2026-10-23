# Magic Portals - the Godot project, on the engine.
#
#   sim/              the port's logic: the level reader and builder, the portal
#                     arithmetic, the player, buttons, doors, crystals and portals,
#                     and the tick they run on. Linked by the tests, so it is
#                     unconditional.
#   MagicPortalsGame  the layer that plays a level on the engine's tick and draws
#                     it. A library, so a test can attach it to a bare registry.
#   MagicPortals      the executable, and
#   spike/            a measurement tool - not a game and not a test. It runs
#                     level30 through the engine's solver and prints the numbers
#                     the spike doc records, each beside a threshold written down
#                     before it ran.
#   Both executables are gated by SUPERSONIC_BUILD_MAGICPORTALS, which the
#   repository's top-level CMakeLists.txt declares, ON by default.
#
# See docs/planning/2026-09-10-magic-portals-port.md.

add_subdirectory(sim)

add_library(MagicPortalsGame STATIC
    MagicPortalsLayer.cpp
)

target_link_libraries(MagicPortalsGame PUBLIC SupersonicCore MagicPortalsSim)
target_include_directories(MagicPortalsGame PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if (MSVC)
    target_compile_options(MagicPortalsGame PRIVATE /W4)
else()
    target_compile_options(MagicPortalsGame PRIVATE -Wall -Wextra)
endif()

if (NOT SUPERSONIC_BUILD_MAGICPORTALS)
    return()
endif()

add_executable(MagicPortals
    main.cpp
    LevelVisit.cpp   # DEV: --visit-levels, never pushed without it
    Route.cpp        # DEV: --route, never pushed without it; test_mp compiles it too, for test_mp_routes
)

# The application manifest (planning step 189): the process's code page is UTF-8, so a path with a character the machine's ANSI page lacks
# (a user name with a c-acute on an English Windows) narrows and opens instead of ending the start with no word. On MagicPortals ALONE; the
# suites and the measurement tool do not carry it. CMake merges it with the linker's own manifest (mt.exe, CMAKE_MT), so the execution level
# stays asInvoker. Windows only: any other platform has no manifest to embed.
if (WIN32)
    target_sources(MagicPortals PRIVATE MagicPortals.manifest)
endif()

target_link_libraries(MagicPortals PRIVATE MagicPortalsGame)

# Where the modern controls look for Magic Rampage's pictures (planning step 184, docs/magic-rampage-controls.md): a
# gitignored folder of this repository, made by tools/art/make_mr_controls.py and READ at run time. The default is
# absolute, because the game changes its working directory into engine/ before it opens anything; the picture files
# are the owner's, never committed, and no packaging script learns about them. The name is one uppercase word after
# MAGICPORTALS_ so that tools/parity/play_build.bat's findstr copies it into a play tree like the others.
get_filename_component(_magicPortalsPadArt "${CMAKE_CURRENT_SOURCE_DIR}/../reference/magic-rampage/controls" ABSOLUTE)
set(SUPERSONIC_MAGICPORTALS_PADART "${_magicPortalsPadArt}"
    CACHE PATH "Magic Rampage's modern-control pictures (reference/magic-rampage/controls in this repository, gitignored)")
# On main.cpp ALONE, as a per-source definition: a PUBLIC one on MagicPortalsSim would recompile every test object
# and relink test_mp for a path no suite reads. main.cpp hands it to the layer as Paths::controls.artDir.
if (MAGICPORTALS_BAKED_PATHS)
    set_source_files_properties(main.cpp PROPERTIES
        COMPILE_DEFINITIONS "MAGICPORTALS_PADART_DIR=\"${SUPERSONIC_MAGICPORTALS_PADART}\"")
else()
    # A release build carries no checkout path: the pad art is found from the asset root at run time (sim/AssetRoot.hpp).
    set_source_files_properties(main.cpp PROPERTIES COMPILE_DEFINITIONS "MAGICPORTALS_PADART_DIR=\"\"")
endif()

# The engine commit this build is configured against, for the first line of the log (planning step 189): a tester's log then says which
# engine the game ran on. Read at CONFIGURE time from the submodule, on main.cpp alone for the reason above, and configure re-runs when the
# submodule's HEAD moves (CMAKE_CONFIGURE_DEPENDS), so moving the pin cannot leave the old hash in a rebuilt exe. "unknown" where git is
# not to be found or the engine is not a checkout.
set(_magicPortalsEngineCommit "unknown")
find_package(Git QUIET)
if (GIT_FOUND)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${CMAKE_CURRENT_SOURCE_DIR}/../engine" rev-parse --short HEAD
                    OUTPUT_VARIABLE _engineHash RESULT_VARIABLE _engineHashResult
                    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if (_engineHashResult EQUAL 0 AND NOT _engineHash STREQUAL "")
        set(_magicPortalsEngineCommit "${_engineHash}")
    endif()
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${CMAKE_CURRENT_SOURCE_DIR}/../engine" rev-parse --absolute-git-dir
                    OUTPUT_VARIABLE _engineGitDir RESULT_VARIABLE _engineGitDirResult
                    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if (_engineGitDirResult EQUAL 0 AND EXISTS "${_engineGitDir}/HEAD")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_engineGitDir}/HEAD")
    endif()
endif()
set_property(SOURCE main.cpp APPEND PROPERTY
    COMPILE_DEFINITIONS "MAGICPORTALS_ENGINE_COMMIT=\"${_magicPortalsEngineCommit}\"")

add_executable(MagicPortalsSpike
    spike/main.cpp
)

target_link_libraries(MagicPortalsSpike PRIVATE MagicPortalsSim)

if (MSVC)
    target_compile_options(MagicPortals PRIVATE /W4)
    target_compile_options(MagicPortalsSpike PRIVATE /W4)
else()
    target_compile_options(MagicPortals PRIVATE -Wall -Wextra)
    target_compile_options(MagicPortalsSpike PRIVATE -Wall -Wextra)
endif()
