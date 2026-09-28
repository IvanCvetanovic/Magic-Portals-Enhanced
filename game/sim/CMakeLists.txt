# Magic Portals' simulation side, ported from the Godot remake.
#
# A library rather than part of an executable, and unconditional, for the reason
# Wolf Brigade's and HUSK's are: the tests link it. Deliberately free of the
# renderer. See docs/planning/2026-09-10-magic-portals-spike.md.

add_library(MagicPortalsSim STATIC
    Achievements.cpp
    Art.cpp
    Boss.cpp
    Bounce.cpp
    Camera.cpp
    Carry.cpp
    Chapters.cpp
    Credits.cpp
    Selector.cpp
    Dashboard.cpp
    Demolish.cpp
    Diamonds.cpp
    Earthquake.cpp
    Ease.cpp
    Effects.cpp
    Fields.cpp
    Fire.cpp
    Follower.cpp
    Game.cpp
    Ghost.cpp
    Goals.cpp
    GravityWell.cpp
    Hazards.cpp
    Hinge.cpp
    Hud.cpp
    Impacts.cpp
    Keys.cpp
    Launchers.cpp
    LevelBuilder.cpp
    LevelEnd.cpp
    Lighting.cpp
    Loading.cpp
    Locking.cpp
    MainMenu.cpp
    MenuState.cpp
    Minions.cpp
    Motion.cpp
    Mover.cpp
    Particles.cpp
    Pause.cpp
    Player.cpp
    Popup.cpp
    Portal.cpp
    Portals.cpp
    Prism.cpp
    Puzzle.cpp
    Roles.cpp
    Scores.cpp
    Shot.cpp
    Sky.cpp
    Sounds.cpp
    Sprites.cpp
    Tiers.cpp
    Trigger.cpp
    Torch.cpp
    Dragon.cpp
    DarkDragon.cpp
    Tscn.cpp
    Turrets.cpp
    UiLayer.cpp
    Zerog.cpp
)

# This repository's root. Every default below is inside it.
get_filename_component(_magicPortalsRepo "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

# Where the converted levels live - this repository's gitignored out/, never
# its history.
#
# They are the original game's level data, decoded by the converter
# (tools/converter) into out/, and they stay on the machine that converted
# them. Anywhere else is one -D away. Baked in as an absolute path because
# ctest runs from the build tree, so a relative path would be right for nobody.
# The suites that read levels skip, and say where they looked, when it is not
# there - see tests/CMakeLists.txt.
set(SUPERSONIC_MAGICPORTALS_LEVELS "${_magicPortalsRepo}/out/levels"
    CACHE PATH "The converted levels (out/levels in this repository)")

# And the remake's own data - its entity_roles.json, portals.json and
# player.json in data/, which are the remake's authored files rather than the
# original's. The tests read the role table and pin what it says of a level
# (test_mp_play); the numbers in portals.json and player.json are marked _guess
# there, so they are carried as data and nothing pins them.
set(SUPERSONIC_MAGICPORTALS_DATA "${_magicPortalsRepo}/data"
    CACHE PATH "The remake's data directory (data in this repository)")

# And the level order, chapters.json: the converter's reading of the original's
# goldenScores.enml, written beside the levels in the same gitignored out/.
set(SUPERSONIC_MAGICPORTALS_CHAPTERS "${SUPERSONIC_MAGICPORTALS_LEVELS}/../data/chapters.json"
    CACHE FILEPATH "The converted level order (out/data/chapters.json in this repository)")

# The port's own data, which IS committed, in game/data: numbers authored for
# the port, such as how much of a level the view shows (view.json).
# And the original's own assets, extracted from its APK into the gitignored
# reference/: the images of what no level places - the portals a shot opens,
# the shot, the player (Art.hpp). Like the levels they are the original's, and
# never committed; drawn without them, those are boxes.
# The default follows the levels, which the converter writes to out/levels.
set(SUPERSONIC_MAGICPORTALS_ORIGINAL "${SUPERSONIC_MAGICPORTALS_LEVELS}/../../reference/extracted/assets"
    CACHE PATH "The original's extracted assets (reference/extracted/assets in this repository)")

# And the original's achievements, achievements.json: tools/asbc/achievements.py's
# reading of the decode, written beside chapters.json in the same gitignored
# out/. Its titles, descriptions, icons and points are the original's, so they
# are never committed; the dashboard draws no rows without the file.
set(SUPERSONIC_MAGICPORTALS_ACHIEVEMENTS "${SUPERSONIC_MAGICPORTALS_LEVELS}/../data/achievements.json"
    CACHE FILEPATH "The original's achievements, extracted (out/data/achievements.json in this repository)")

target_compile_definitions(MagicPortalsSim PUBLIC
    MAGICPORTALS_ORIGINAL_DIR="${SUPERSONIC_MAGICPORTALS_ORIGINAL}"
    MAGICPORTALS_LEVELS_DIR="${SUPERSONIC_MAGICPORTALS_LEVELS}"
    MAGICPORTALS_DATA_DIR="${SUPERSONIC_MAGICPORTALS_DATA}"
    MAGICPORTALS_CHAPTERS_FILE="${SUPERSONIC_MAGICPORTALS_CHAPTERS}"
    MAGICPORTALS_ACHIEVEMENTS_FILE="${SUPERSONIC_MAGICPORTALS_ACHIEVEMENTS}"
    MAGICPORTALS_PORT_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../data")

target_include_directories(MagicPortalsSim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(MagicPortalsSim PUBLIC SupersonicCore)

if (MSVC)
    target_compile_options(MagicPortalsSim PRIVATE /W4)
else()
    target_compile_options(MagicPortalsSim PRIVATE -Wall -Wextra)
endif()
