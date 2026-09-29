# The port's suites, verified against the Godot remake and the original.
#
# Each suite is one .cpp built by the engine's own rule, supersonic_add_test
# (engine/cmake/SupersonicTesting.cmake): the test harness, SupersonicCore and
# the GLM configuration its interface carries, /W4, and add_test under the
# suite's own name, so ctest runs it from build/tests. The engine's suites are
# built by the same function in the engine's own build and are not built here.
#
# Magic Portals, from the Godot remake. The weakest oracle of the three: the
# remake's harnesses assert behaviour over Godot's 2D solver, which is not being
# reimplemented, so only what the converter and the role table decide is pinned
# exactly. The acceptance rule is in docs/planning/2026-09-10-magic-portals-spike.md.
#
# SKIPS_WITH_77 marks a suite that reads the converted levels - this repository's
# gitignored out/, which a fresh clone does not have - some of them chapters.json
# or the remake's data as well, so on a build machine they can simply be absent.
# Such a suite exits 77 before its first check, printing the path it looked in,
# and ctest reports it SKIPPED: never a pass. But ctest knows 77 only from the
# test's SKIP_RETURN_CODE, and without it the same exit is a FAIL. The mark sits
# on the suite's own line because a separate list of names is what nine suites
# were added without, so a fresh clone would report them failed. A misspelt mark
# stops the configure rather than being dropped; only a word CMake reads as false
# (OFF, NO, 0, IGNORE...) gets through. A suite that skips only a part of itself
# and still returns its summary takes no mark: that part is skipped without the
# data, and never with 77.
function(add_mp_test name)
    cmake_parse_arguments(PARSE_ARGV 1 MP "SKIPS_WITH_77" "" "")
    if (MP_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "add_mp_test(${name}): unknown argument ${MP_UNPARSED_ARGUMENTS}")
    endif()
    supersonic_add_test(${name})
    target_link_libraries(${name} PRIVATE MagicPortalsSim)
    if (MP_SKIPS_WITH_77)
        set_tests_properties(${name} PROPERTIES SKIP_RETURN_CODE 77)
    endif()
endfunction()

add_mp_test(test_mp_tscn)
add_mp_test(test_mp_levels SKIPS_WITH_77)
add_mp_test(test_mp_geometry)
add_mp_test(test_mp_portal)
add_mp_test(test_mp_play SKIPS_WITH_77)

# The carrancas of chapter 2, and the fireballs they spit. Reads the converted
# levels and the remake's role table, so it skips with 77 when they are absent.
add_mp_test(test_mp_turrets SKIPS_WITH_77)

# The medals, kept between runs. Pure sim: a temporary directory and no window.
# The ONE Magic Portals suite that touches a filesystem, and it touches only its
# own scratch directory - the layer suites all build bare, which is what
# Paths::saveDir being empty by default buys.
add_mp_test(test_mp_scores)
# Every converted level started as the port starts one: the remaster's inventory.
add_mp_test(test_mp_start SKIPS_WITH_77)
# Static portals, on the tutorial levels that ship them.
add_mp_test(test_mp_statics SKIPS_WITH_77)
# The level order (chapters.json), and the camera's arithmetic.
add_mp_test(test_mp_chapters SKIPS_WITH_77)
add_mp_test(test_mp_camera)
# Timed crystals, which go a set time after a level starts.
add_mp_test(test_mp_timed SKIPS_WITH_77)
# Moving platforms, lifts, and no-portal zones that patrol.
add_mp_test(test_mp_movers SKIPS_WITH_77)
# Hazards, and where they kill.
add_mp_test(test_mp_hazards SKIPS_WITH_77)
# Rolling stones, and the walls they break.
add_mp_test(test_mp_demolish SKIPS_WITH_77)
# Fire agents, burning crates and barrel bombs - and the asymmetry between them,
# that fire kills the player where a blast cannot reach one.
add_mp_test(test_mp_fire SKIPS_WITH_77)
# Seesaws and spinning platforms. No tick of its own: the joint is attached when
# the level is built and the engine's solver does the rest.
add_mp_test(test_mp_hinge SKIPS_WITH_77)
# The minions of worlds 3 and 4: spawned from a marker, walking a prefix of
# waypoints, and taken by the floor that kills them and nothing else.
add_mp_test(test_mp_minions SKIPS_WITH_77)
# Chapter 3's boss: the ghost of level 3-32, which only fire can hurt and which
# drops the only key its level has.
add_mp_test(test_mp_ghost SKIPS_WITH_77)
# Chapter 4's torches, and the wall of light one drops. The inventory counts
# roles and light_wall.ent has none, so only this suite can tell a played level
# from one with an unremovable solid across it.
add_mp_test(test_mp_torch SKIPS_WITH_77)
# Chapter 4's first eighteen levels, where gravity is zero, the walking buttons
# do nothing, and the recoil of a portal shot is the only way to move. The one
# remaining level property, and the last thing stopping a level starting.
add_mp_test(test_mp_zerog SKIPS_WITH_77)
# The slabs that bob in chapter 4's weightless rooms. One pixel of travel, and
# the suite watches the BODY is kinematic - a static one would carry nothing.
add_mp_test(test_mp_bounce SKIPS_WITH_77)
# Chapter 4's gravity wells: a solid circle that pulls every dynamic body in, and
# the no-portal zone its callback adds that no level file mentions.
add_mp_test(test_mp_wells SKIPS_WITH_77)
# Chapter 2's boss: the dragon that cannot be killed, and the claw that eats its
# level's floor behind a camera locked to the dragon itself. The inventory counts
# roles, and the claw's four names have none - so only this suite can tell a
# level whose floor really goes from one where the claw is drawn and inert.
add_mp_test(test_mp_dragon SKIPS_WITH_77)
# Chapter 4's boss, and the last level of the game: summoned by the player's own
# light, hurt only by a barrel bomb its own fireball sets off, and the holder of
# the only key its level has.
add_mp_test(test_mp_darkdragon SKIPS_WITH_77)
# Keys, the keyholes they open and the locked doors that go with them - paired by
# colour, and picked up at a distance rather than on contact.
add_mp_test(test_mp_keys SKIPS_WITH_77)
# Shock diamonds: the carried thing only a character may hold, which kills the
# first minion it comes within range of and deletes itself. Selected by entity
# name rather than by role, because the fire diamond shares the role.
add_mp_test(test_mp_diamonds SKIPS_WITH_77)
# Shock fields: a lethal ring that swings about the node that placed it, polled
# every frame rather than entered. The node is the CENTRE of the swing.
add_mp_test(test_mp_fields SKIPS_WITH_77)
# Launchers, what they throw, and what takes it back.
add_mp_test(test_mp_launchers SKIPS_WITH_77)
# The portal shot: a tap fires, and the portal opens where the shot arrives.
add_mp_test(test_mp_shot SKIPS_WITH_77)
# Chapter 1's boss, the beholder of level31, as the original's script plays it.
add_mp_test(test_mp_boss SKIPS_WITH_77)
# A level's own art: the sprites the converter wrote, their order and blend.
# Its reader is tested on files it writes, so it runs anywhere; only the part
# that reads the converted levels is skipped without them, and never with 77.
add_mp_test(test_mp_sprites)
# A level's lighting: the converter's eth_* keys, read strictly and drawn by
# nothing yet (sim/Lighting). Like test_mp_sprites, the reader is tested on files
# it writes and runs anywhere; only the part that reads the converted levels is
# skipped without them, and never with 77.
add_mp_test(test_mp_lighting)
# The sky the original pins to the camera: sky.json and sim/Sky's arithmetic, pure,
# and a census of what the converted levels place, which is skipped without them
# and never with 77.
add_mp_test(test_mp_sky)
# The texture tiers and the one cut rule: tiers.json and sim/Tiers - which file of an
# image the original draws, its size in units and its frames' uv - on files the suite
# writes, and over the original's own tier folders, which are skipped without them and
# never with 77.
add_mp_test(test_mp_tiers)
# The small motions the original's scripts give an entity: motions.json and sim/Motion -
# linearMotion's step, clamp, wrap, cos phase and axis, and the crystals' and keys' bob -
# pure, and a census of the converted levels' crystals and keys, which is skipped
# without them and never with 77.
add_mp_test(test_mp_motion)
# The effects the original's script adds: effects.json and sim/Effects - each call's
# entity, offset, depth, turn and scale, and the arithmetic of where the teleport's
# three land. Pure, reading only the port's own committed data, so it never skips.
add_mp_test(test_mp_effects)
# The sounds: the decoded AudioManager as data, and that every file it names is
# really in the original's soundfx/. The reader runs anywhere; only the part
# that opens the original's own files is skipped without them, never with 77.
add_mp_test(test_mp_sounds)

# The HUD a level is played under and the seconds it opens with - ui.json and
# sim/Hud's arithmetic, pinned against the captures of the original. Pure, and
# reads only the port's own committed data, so it runs anywhere and never skips.
add_mp_test(test_mp_hud)

# How a level ends: the finished and lost screens and the HUD going - ui.json's
# level_end and sim/LevelEnd's arithmetic, pinned against the remake's ui2 spec.
# Pure, reading only the port's own committed data, so it never skips.
add_mp_test(test_mp_levelend)

# The win cutscenes and the delay before the medal (planning step 154):
# cutscenes.json and sim/Cutscene - 4-01's launch, 4-19's landing, 2-32's knight
# and end_delay. The arithmetic reads only the port's own committed data; the
# census and the knight read the converted levels and are skipped without them,
# never with 77.
add_mp_test(test_mp_cutscene)

# The tutorial and help popups: ui.json's popups and sim/Popup's arithmetic - the
# framework, every class's waypoint loops, the close button's bounce and blink -
# pinned against the remake's ui2 spec, section 5. Pure, and it never skips.
add_mp_test(test_mp_popup)

# The front door: the loading screen, the main menu, and the black, press, bounce
# and blink every menu state shares - ui.json's menu_state, main_menu and loading
# and sim/MenuState, sim/MainMenu and sim/Loading, pinned against the remake's ui3
# spec, sections 0 and 2. Pure, reading only the port's own committed data, so it
# never skips.
add_mp_test(test_mp_frontdoor)

# Credits and the achievements dashboard, and what the save has opened: ui.json's
# credits, dashboard and locking with sim/Credits, sim/Dashboard, sim/Achievements and
# sim/Locking, pinned against the remake's ui3 spec, sections 3 and 4. Pure, and it
# reads only the port's own committed data - its achievement lists are its own, never
# the original's - so it never skips.
add_mp_test(test_mp_info)
add_mp_test(test_mp_select)

# The layer, attached to a bare registry: no window, no Vulkan. It drives taps
# through the camera, and plays level30 from the spawn to the exit.
add_mp_test(test_mp_layer SKIPS_WITH_77)
target_link_libraries(test_mp_layer PRIVATE MagicPortalsGame)
