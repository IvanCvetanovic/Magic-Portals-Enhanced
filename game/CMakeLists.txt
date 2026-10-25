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

# LevelVisit.cpp (--visit-levels) and Route.cpp (--route) are the developer tools' files and are in the program only with MAGICPORTALS_DEV_TOOLS
# (top-level CMakeLists.txt); test_mp compiles Route.cpp on its own, for test_mp_routes, whatever this says.
set(_magicPortalsSources main.cpp)
if (MAGICPORTALS_DEV_TOOLS_ENABLED)
    list(APPEND _magicPortalsSources
        LevelVisit.cpp   # DEV: --visit-levels, never pushed without it
        Route.cpp        # DEV: --route, never pushed without it; test_mp compiles it too, for test_mp_routes
    )
endif()
if (ANDROID)
    # A NativeActivity loads the game as libMagicPortals.so (android/AndroidManifest.xml names it). The engine's android_main, linked in with SupersonicCore,
    # enters the game through SupersonicMain (android/AndroidMain.cpp), which unpacks the APK's data and calls main.cpp's MagicPortalsMain as main() does on a
    # desktop. The TARGET keeps its name, so that the two definitions below (MAGICPORTALS_MOBILE and MAGICPORTALS_DEV_TOOLS) reach AndroidMain.cpp by the very
    # line that gives them to main.cpp (planning steps 201 to 203).
    add_library(MagicPortals SHARED ${_magicPortalsSources} android/AndroidMain.cpp)
elseif (IOS)
    # The engine's main() (engine/src/platform/ios/IOSApp.mm, in SupersonicCore) starts UIKit and enters the game through SupersonicMain, which ios/IOSMain.mm
    # defines: it points the game at the data in the app bundle and calls main.cpp's MagicPortalsMain. A plain executable: tools/apple/make_app.sh puts it, Info.plist
    # and the files into MagicPortals.app (planning step 206, PORT-8; written, never run).
    add_executable(MagicPortals ${_magicPortalsSources} ios/IOSMain.mm)
elseif (APPLE)
    # main() is macos/MacMain.mm's: inside a MagicPortals.app (tools/apple/make_app.sh) it finds the data in Contents/Resources and puts the engine's shaders
    # where it may write, then calls MagicPortalsMain as everywhere else (planning step 206, PORT-8; written, never run).
    add_executable(MagicPortals ${_magicPortalsSources} macos/MacMain.mm)
else()
    add_executable(MagicPortals ${_magicPortalsSources})
endif()

# The application manifest (planning step 189): the process's code page is UTF-8, so a path with a character the machine's ANSI page lacks
# (a user name with a c-acute on an English Windows) narrows and opens instead of ending the start with no word. On MagicPortals ALONE; the
# suites and the measurement tool do not carry it. CMake merges it with the linker's own manifest (mt.exe, CMAKE_MT), so the execution level
# stays asInvoker. Windows only: any other platform has no manifest to embed.
if (WIN32)
    target_sources(MagicPortals PRIVATE MagicPortals.manifest)
endif()

target_link_libraries(MagicPortals PRIVATE MagicPortalsGame)

# THE APPLE PROGRAMS (planning step 206, PORT-8): everything in this block is behind APPLE, so with it false the generated build files are the same.
if (APPLE)
    # ARC manages the entries' Objective-C objects, as the engine's iOS file's (the sibling port's own line).
    set_source_files_properties(macos/MacMain.mm ios/IOSMain.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    target_link_libraries(MagicPortals PRIVATE "-framework Foundation")
    if (NOT IOS)
        # NSAlert, the macOS entry's words to a player that has no game data. Named here, not left to the frameworks MoltenVK's link line happens to carry.
        target_link_libraries(MagicPortals PRIVATE "-framework AppKit")
    endif()
    # main.cpp defines main() everywhere but on Android (its last lines), and an Apple program already has one: macOS's is macos/MacMain.mm's, iOS's is in the
    # engine's SupersonicCore (IOSApp.mm). A second `_main` is a duplicate symbol in both links. main.cpp is a source of the Windows program and stays as it
    # is; its one `main(` is renamed away for Apple targets here, per source, by a header that is force-included into it alone (apple/RenameDesktopMain.hpp says why it
    # is a file and not a -D: CMake drops a function-like one), so that MagicPortalsMain (the whole game) is the only thing it defines that anyone calls. The one-line
    # source guard that would replace this (#if !defined(__ANDROID__) && !defined(__APPLE__)) waits for a step that may relink the Windows program (docs/apple.md,
    # planning step 206, Left For Later).
    set_property(SOURCE main.cpp APPEND PROPERTY COMPILE_OPTIONS "-include;${CMAKE_CURRENT_SOURCE_DIR}/apple/RenameDesktopMain.hpp")
    # The bundle's Info.plist, from the template of the target, with the program's ONE version (project(MagicPortals VERSION ...) at the top) in it: written
    # to this directory of the build tree, where tools/apple/make_app.sh reads it.
    set(MAGICPORTALS_BUNDLE_VERSION "${MagicPortals_VERSION}")
    if (IOS)
        configure_file(ios/Info.plist.in "${CMAKE_CURRENT_BINARY_DIR}/ios/Info.plist" @ONLY)
    else()
        configure_file(macos/Info.plist.in "${CMAKE_CURRENT_BINARY_DIR}/macos/Info.plist" @ONLY)
    endif()
endif()

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

# What kind of program this is (BuildProfile.hpp, planning step 201): its two facts, spelled out whatever their value, on EVERY source file of the
# MagicPortals target and on nothing else (planning step 202). Step 201 gave them to main.cpp alone, which left a second file of the program that
# included the header free to disagree with it; the header now stops a translation unit that has neither the facts nor the suites' declaration.
# The libraries and the suites are not given them, so none of those is recompiled when they change.
set(_magicPortalsMobileValue 0)
if (MAGICPORTALS_MOBILE)
    set(_magicPortalsMobileValue 1)
endif()
set(_magicPortalsDevToolsValue 0)
if (MAGICPORTALS_DEV_TOOLS_ENABLED)
    set(_magicPortalsDevToolsValue 1)
endif()
# The performance-diagnostic build (planning step 205): a third fact, on the same footing, independent of the other two.
set(_magicPortalsPerfDiagValue 0)
if (MAGICPORTALS_PERF_DIAG)
    set(_magicPortalsPerfDiagValue 1)
endif()
target_compile_definitions(MagicPortals PRIVATE
    "MAGICPORTALS_MOBILE=${_magicPortalsMobileValue}" "MAGICPORTALS_DEV_TOOLS=${_magicPortalsDevToolsValue}" "MAGICPORTALS_PERF_DIAG=${_magicPortalsPerfDiagValue}")

# The Windows release program (MAGICPORTALS_WIN_GUI, top-level CMakeLists.txt): a function of its own, so the rehearsal of the link calls the same code.
if (MAGICPORTALS_WIN_GUI)
    include(${CMAKE_CURRENT_SOURCE_DIR}/windows/MagicPortalsWindows.cmake)
    magicportals_windows_release(MagicPortals VERSION "${MagicPortals_VERSION}" ICON "${MAGICPORTALS_ICON}")
endif()

if (MSVC)
    target_compile_options(MagicPortals PRIVATE /W4)
else()
    target_compile_options(MagicPortals PRIVATE -Wall -Wextra)
endif()

# The measurement tool is a developer's, and unguarded in its use of the system's temporary folder: never a phone's (planning step 201).
if (NOT MAGICPORTALS_MOBILE)
    add_executable(MagicPortalsSpike
        spike/main.cpp
    )

    target_link_libraries(MagicPortalsSpike PRIVATE MagicPortalsSim)

    if (MSVC)
        target_compile_options(MagicPortalsSpike PRIVATE /W4)
    else()
        target_compile_options(MagicPortalsSpike PRIVATE -Wall -Wextra)
    endif()
endif()
