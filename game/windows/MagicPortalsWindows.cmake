# The Windows RELEASE executable (planning step 201, PORT-3): what a player's MagicPortals.exe is, as one function over a target.
#
#   magicportals_windows_release(<target> VERSION <x.y.z> [ICON <file.ico>])
#
# It is a function in a file of its own, and not a block in game/CMakeLists.txt, so that the rehearsal of the link (a toy executable that is
# never launched) calls the SAME code the game's does; and it is called only when MAGICPORTALS_WIN_GUI is ON, so a developer's build/ and every
# parity script keep the console program they have always had (a GUI-subsystem program has no console for a script's stdout and --help).
#
# What it makes of the target:
#   - a GUI program, not a console one (WIN32_EXECUTABLE ON): double-clicked it opens no black console window behind the game. main() stays
#     the entry point (/ENTRY:mainCRTStartup; the default entry of a GUI program is WinMainCRTStartup, which wants a WinMain). A shell that
#     redirects stdout and stderr to a file or a pipe still receives them: the standard handles are inherited, the subsystem does not change
#     that. Started from Explorer it has none, and the game's log (sim/Scratch.hpp) is where its words go.
#   - the Vulkan loader (vulkan-1.dll) LOADED LATE (/DELAYLOAD plus delayimp.lib): it is found at the first Vulkan call instead of at process start.
#     Loaded at process start, a PC with no graphics driver never reaches main(): Windows refuses to start the program, with a dialog that names
#     a DLL and nothing else. Late, main() asks first (sim/StartChecks.hpp, game/main.cpp) and says in words what to install. The loader's NAME has
#     ONE definition, the C++ constant main() asks for (kVulkanLoaderName in sim/StartChecks.hpp): it is read out of that header below, so the
#     library that is delayed and the library that is asked for cannot differ (planning step 202; two spellings of it were Step 201's finding).
#   - a VERSIONINFO resource (the file's properties, which Explorer shows and Windows' Smart App Control reads as part of what an unsigned
#     program looks like) and, only when ICON names a file, that icon. The application manifest is NOT embedded here: game/MagicPortals.manifest
#     is a source of the target and the linker merges it (CMake passes it as /MANIFESTINPUT); a second one would collide.
#
# MSVC only (the linker options are its own). Anything else is a configure error, not a silent no-op: a release that quietly lacks them is worse.

set(_MAGICPORTALS_WINDOWS_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(_MAGICPORTALS_STARTCHECKS_HEADER "${CMAKE_CURRENT_LIST_DIR}/../sim/StartChecks.hpp")

function(magicportals_windows_release target)
    cmake_parse_arguments(PARSE_ARGV 1 MPW "" "VERSION;ICON" "")
    if (MPW_UNPARSED_ARGUMENTS OR NOT MPW_VERSION)
        message(FATAL_ERROR "magicportals_windows_release(<target> VERSION <x.y.z> [ICON <file.ico>])")
    endif()
    if (NOT (WIN32 AND MSVC))
        message(FATAL_ERROR "MAGICPORTALS_WIN_GUI is the Windows release build and is made with MSVC (link.exe's /ENTRY, /DELAYLOAD and delayimp.lib); "
                            "this configure is not one. Leave it OFF here.")
    endif()

    # THE LOADER'S NAME, read from the one place that defines it (see the header of this file). The declaration is matched whole, so a comment that
    # shows the shape cannot be taken for it, and a header that no longer has it stops the configure: a /DELAYLOAD of a guessed name would be worse.
    # Configure re-runs when the header changes, so the link line follows an edit of the constant.
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_MAGICPORTALS_STARTCHECKS_HEADER}")
    file(STRINGS "${_MAGICPORTALS_STARTCHECKS_HEADER}" _loaderLine REGEX [[^inline constexpr const char\* kVulkanLoaderName = "[^"]+";]])
    list(LENGTH _loaderLine _loaderLines)
    if (NOT _loaderLines EQUAL 1 OR NOT "${_loaderLine}" MATCHES [[kVulkanLoaderName = "([^"]+)"]])
        message(FATAL_ERROR "${_MAGICPORTALS_STARTCHECKS_HEADER} does not declare the Vulkan loader's name as exactly one line of the form\n"
                            "  inline constexpr const char* kVulkanLoaderName = \"<name>\";\n"
                            "(found ${_loaderLines}). The Windows release delays that library's load, so the build needs to read the name from there.")
    endif()
    set(_vulkanLoader "${CMAKE_MATCH_1}")

    # "0.1.0" -> FILEVERSION 0,1,0,0 and the string "0.1.0.0": the four fields a VERSIONINFO has. A project version of fewer fields is padded.
    string(REPLACE "." ";" _fields "${MPW_VERSION}")
    list(LENGTH _fields _count)
    while (_count LESS 4)
        list(APPEND _fields 0)
        math(EXPR _count "${_count} + 1")
    endwhile()
    list(GET _fields 0 _major)
    list(GET _fields 1 _minor)
    list(GET _fields 2 _patch)
    list(GET _fields 3 _build)
    set(MAGICPORTALS_RC_VERSION "${_major}.${_minor}.${_patch}.${_build}")
    set(MAGICPORTALS_RC_VERSION_COMMAS "${_major},${_minor},${_patch},${_build}")
    set(MAGICPORTALS_RC_NAME "${target}")

    # The icon is OPTIONAL and is never part of this repository (the hard rule: no image file is committed, an icon included). Given a file it is
    # embedded as resource 1; given none the resource is simply not there. A path that names nothing is an error, never a build without it that
    # the person did not ask for. Forward slashes: a resource compiler reads a backslash inside a string as an escape.
    set(MAGICPORTALS_RC_ICON_LINE "")
    if (MPW_ICON)
        if (NOT EXISTS "${MPW_ICON}")
            message(FATAL_ERROR "MAGICPORTALS_ICON names no file: ${MPW_ICON}")
        endif()
        file(TO_CMAKE_PATH "${MPW_ICON}" _icon)
        set(MAGICPORTALS_RC_ICON_LINE "1 ICON \"${_icon}\"")
    endif()

    set(_rc "${CMAKE_CURRENT_BINARY_DIR}/${target}.rc")
    configure_file("${_MAGICPORTALS_WINDOWS_DIR}/MagicPortals.rc.in" "${_rc}" @ONLY)
    target_sources(${target} PRIVATE "${_rc}")
    if (MPW_ICON)
        # The icon is read when the resource is compiled, so a new icon must rebuild it.
        set_source_files_properties("${_rc}" PROPERTIES OBJECT_DEPENDS "${MPW_ICON}")
    endif()

    set_target_properties(${target} PROPERTIES WIN32_EXECUTABLE ON)
    target_link_options(${target} PRIVATE /ENTRY:mainCRTStartup "/DELAYLOAD:${_vulkanLoader}")
    target_link_libraries(${target} PRIVATE delayimp)
endfunction()
