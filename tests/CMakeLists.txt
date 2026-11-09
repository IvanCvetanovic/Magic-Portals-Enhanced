# The port's suites, verified against the Godot project and the original.
#
# Each suite is one .cpp, and all of them are built into ONE executable, test_mp
# (test_mp.cpp): `test_mp <suite>` runs one suite as its own executable did - its
# output, its summary line, its exit code - and `test_mp` alone runs every suite.
# Each suite keeps its own ctest entry under its own name, `test_mp <suite>`, run
# from build/tests as before. MpSuite.hpp is how a suite's main registers.
#
# A deliberate departure from the engine's rule, supersonic_add_test
# (engine/cmake/SupersonicTesting.cmake), which makes one executable per suite,
# and which the port followed until planning step 129. The reason is Windows
# Smart App Control on the owner's machine: it refuses the first launch of a share
# of freshly linked, unsigned executables, and every refusal is a notification to
# the owner. Every suite links MagicPortalsSim, so a change to a widely included
# header relinked every suite - 47 of them then (wave 1's merge alone drew 41 refusals). One executable
# makes a relink one new binary. What the rule gives a suite is kept on test_mp by
# hand, below: the harness, SupersonicCore and the GLM configuration its interface
# carries, and /W4. That copy is what the engine's rule exists to avoid, so if
# supersonic_add_test changes, this block follows it. The engine's own suites are
# built by that rule in the engine's own build and are not built here.
#
# Magic Portals, from the Godot project. The weakest oracle of the three: the
# Godot project's harnesses assert behaviour over Godot's 2D solver, which is not being
# reimplemented, so only what the converter and the role table decide is pinned
# exactly. The acceptance rule is in docs/planning/2026-09-10-magic-portals-spike.md.
#
# SKIPS_WITH_77 marks a suite that reads the converted levels - this repository's
# gitignored out/, which a fresh clone does not have - some of them chapters.json
# or Enhanced's data as well, so on a build machine they can simply be absent.
# Such a suite returns 77 before its first check, printing the path it looked in;
# test_mp exits with it, and ctest reports it SKIPPED: never a pass. (`test_mp`
# over every suite counts a 77 as skipped too.) But ctest knows 77 only from the
# test's SKIP_RETURN_CODE, and without it the same exit is a FAIL. The mark sits
# on the suite's own line because a separate list of names is what nine suites
# were added without, so a fresh clone would report them failed. A misspelt mark
# stops the configure rather than being dropped; only a word CMake reads as false
# (OFF, NO, 0, IGNORE...) gets through. A suite that skips only a part of itself
# and still returns its summary takes no mark: that part is skipped without the
# data, and never with 77. That is for a small part. Where the part is large, or a
# run without the data would otherwise count it as passed, the part is a suite of its
# own that exits 77 (add_mp_test(<name> IN_SOURCE <file> SKIPS_WITH_77) below, planning
# step 209): test_mp_shoot_levels, test_mp_sprites_original and test_mp_sprites_levels.
#
# The layer (MagicPortalsGame) and game/Route.cpp are linked for every suite now,
# where only test_mp_layer and test_mp_routes had them: the static library adds
# only what a suite calls, and only the routes suite calls Route.cpp.
add_executable(test_mp test_mp.cpp ${CMAKE_SOURCE_DIR}/game/Route.cpp)
target_link_libraries(test_mp PRIVATE Supersonic::TestHarness SupersonicCore MagicPortalsSim MagicPortalsGame)
if (MSVC)
    target_compile_options(test_mp PRIVATE /W4)
else()
    target_compile_options(test_mp PRIVATE -Wall -Wextra)
endif()

# IN_SOURCE <suite>: this suite is the second (third) of a file that add_mp_test(<suite>) already compiled in, registered under its own name by a MpSuite::Registration of its
# own in that file (planning step 209, PORT-11): the part of a suite that needs the owner's data is a suite of its own that exits 77, so that a run without the data
# reports it SKIPPED and not passed. The line keeps the form add_mp_test(<name> IN_SOURCE <file> SKIPS_WITH_77), which the macOS job's count of marked suites reads.
function(add_mp_test name)
    cmake_parse_arguments(PARSE_ARGV 1 MP "SKIPS_WITH_77" "IN_SOURCE" "")
    if (MP_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "add_mp_test(${name}): unknown argument ${MP_UNPARSED_ARGUMENTS}")
    endif()
    if (MP_IN_SOURCE)
        get_target_property(_mpSources test_mp SOURCES)
        list(FIND _mpSources ${MP_IN_SOURCE}.cpp _mpAt)
        if (_mpAt EQUAL -1)
            message(FATAL_ERROR "add_mp_test(${name} IN_SOURCE ${MP_IN_SOURCE}): ${MP_IN_SOURCE}.cpp is not compiled into test_mp yet, so there is no file for ${name} to be in")
        endif()
    else()
        target_sources(test_mp PRIVATE ${name}.cpp)
    endif()
    add_test(NAME ${name} COMMAND test_mp ${name})
    if (MP_SKIPS_WITH_77)
        set_tests_properties(${name} PROPERTIES SKIP_RETURN_CODE 77)
    endif()
endfunction()

add_mp_test(test_mp_tscn)

# The number parser every platform shares (planning step 199, PORT-1): sim/Numbers' ParseDouble, std::from_chars where the library has the
# floating-point overload and a fallback where it does not (the Android NDK's libc++, Apple's). The table, the oracle and the fuzz are pure;
# the part that reads every number of the converted levels and the data skips itself without them, so the suite takes no SKIPS_WITH_77.
add_mp_test(test_mp_numbers)
add_mp_test(test_mp_levels SKIPS_WITH_77)

# UTF-8 text and the interface languages (planning step 236, PORT-32): sim/Utf8's decoder and encoder (each malformed class pinned to its exact
# output, a fuzz for the round trip), sim/Strings' language table and its fourteen keys in eleven languages (key and placeholder parity, the
# original fonts' character set, the drift against the English the rules fields still hold, fallback to English), and sim/Language's saved
# choice. Pure sim and a scratch directory; its drift case ties en.json to the English the rule tables still hold (the secret achievement's words are
# Asantee's, in no committed table: the overlay's, test_mp_langsim).
add_mp_test(test_mp_utf8)
add_mp_test(test_mp_strings)
add_mp_test(test_mp_language)

# Fonts for the languages the original's bitmaps cannot spell (planning step 237, PORT-33): sim/ArabicShaping (the joining forms, the lam-alef
# ligature and the bidirectional reordering, worked by hand from Penumbra's cases and this game's own strings) and sim/FontBake (the atlas baked
# from the committed OFL faces: every glyph present, whole-pixel advances, determinism, the cache that writes nothing, the engine's own reader
# loading it, and a coverage check of every string the shipped tables draw). The achievements' translations are not in git: that part of the
# coverage says so when out/data/achievements.<code>.json is absent (a part of a suite, so no SKIPS_WITH_77).
add_mp_test(test_mp_arabic)
add_mp_test(test_mp_fontbake)
add_mp_test(test_mp_geometry)
add_mp_test(test_mp_portal)
add_mp_test(test_mp_play SKIPS_WITH_77)

# The carrancas of chapter 2, and the fireballs they spit. Reads the converted
# levels and Enhanced's role table, so it skips with 77 when they are absent.
add_mp_test(test_mp_turrets SKIPS_WITH_77)

# The medals and the sound and music switches, kept between runs. Pure sim: a
# temporary directory and no window. The Magic Portals suite that touches a
# filesystem, and it touches only its own scratch directory - the layer suites
# build bare, which is what Paths::saveDir being empty by default buys. One layer
# case gives a save directory, its own (test_mp_layer's
# TheSwitchesAreKeptBetweenRuns, planning step 124).
add_mp_test(test_mp_scores)

# What keeps a player's saves whole and a start from ending in silence (planning step 189): sim/Files' one atomic write as the four stores
# use it, the .bak of the last good file, a directory for the log found without throwing, and the rule of which runs may open a message box.
# Pure sim and a scratch directory; no converted level, so no skip.
add_mp_test(test_mp_robust)

# Where the game finds its data and where it may write (planning step 200, PORT-2): sim/AssetRoot's order of tiers (--root, next to the
# program, the bundle, the user's data folder, the paths the build was configured with), the per-path flags over its answer, and that a
# resolve only looks; sim/Scratch's rule that the log and the prisms go to the first folder a file can really be made in; and the prism
# writer's half-written name. Pure: opaque fixture folders and a scratch directory of its own, no converted level, so no skip.
add_mp_test(test_mp_assetroot)

# What the run asks of the engine's optional performance features (planning step 190): game/data/engine.json as sim/EngineTuning reads
# it, strictly, for a person's run and for every other, and the flags that override it for one run; and (planning step 201) the phone
# profile laid over it, which for a desktop run changes nothing, pinned against a copy of the rule as it was. Pure sim and a scratch directory.
add_mp_test(test_mp_tuning)

# What the start checks before it touches the engine, and what a build without the developer tools says (planning step 201, PORT-3): the Vulkan
# loader question asked of an injected loader, in words that name what is missing and what to do; the list of the developer's flags a build
# without them refuses; the two build facts main.cpp is given. Pure.
add_mp_test(test_mp_platform)

# The first-run unpack the Android app does with its APK's data (planning step 203, PORT-5): sim/Unpack's list format and the rules a name in it must pass,
# the order of a run (the stamp removed first and written last, space checked from the list, the pump, a cancel that is not a failure), the folder sink
# on a real scratch directory, and the developer's flags file. Pure: invented lists, an in-memory APK and disk, no original file.
add_mp_test(test_mp_unpack)

# Where the game's data is in a macOS or iOS bundle and what the two Apple entries do with the command line (planning step 206, PORT-8): sim/AppleLayout's two layouts and
# the order they are looked in, what holds the game (tied to the packaged shape), the arguments the game is handed (--root, absolute path flags), whether a run may open
# a dialog, and the words of a start that found no data. The entries themselves (game/macos, game/ios) are Objective-C++ that no host here can compile: this suite is the only
# thing that runs their decisions, and the three ties it makes read the entries' text. Pure; it reads three source files through this one source's MAGICPORTALS_SOURCE_DIR
# and nothing else, so it never skips.
add_mp_test(test_mp_applelayout)
set_property(SOURCE test_mp_applelayout.cpp APPEND PROPERTY
             COMPILE_DEFINITIONS "MAGICPORTALS_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\"")

# The CI fixture level, tests/fixtures/ci (planning step 206, PORT-8): a level written from scratch, hand-written .tscn and .json only, that a runner with none of the game's
# data starts with `MagicPortals --root tests/fixtures/ci --port-data game/data --level ci_level ...`. The suite pins what the folder may hold (no picture, no sound, no name of the original's), that
# the two tables of Enhanced's copied into it are the repository's byte for byte (tools/ci/sync_fixture_tables.py refreshes them), and that the level reads through the game's readers,
# starts, and its gate opens when its plate is pressed. Pure, no window, no converted level: it never skips.
add_mp_test(test_mp_cifixture)
set_property(SOURCE test_mp_cifixture.cpp APPEND PROPERTY
             COMPILE_DEFINITIONS "MAGICPORTALS_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\"")

# How a level is started on a phone (planning step 205, PORT-7): sim/LevelStart's state machine, the cover that is drawn before the load and the rule that
# ignores a finger that went down before the new level's first frame, until it lifts. Scripts of the machine's five inputs and the traces they must give; pure.
add_mp_test(test_mp_levelstart)

# The same rule through the real layer (planning step 205): a bare layer with Paths::coverLevelStarts, ticked and drawn as the app does, walked from the menu to a tile:
# nothing loads in the asking frame, the next frame is a single black quad, the load runs in the tick after it, and a press made under the cover or queued during the load
# fires no shot. Needs the converted levels like test_mp_layer, and is skipped (77) without them.
add_mp_test(test_mp_startflow SKIPS_WITH_77)

# The original's script clock: the whole milliseconds a frame gives the scripts, as a floor, and
# what stays on the physics step and on the wall clock (planning step 147). Pure arithmetic.
add_mp_test(test_mp_clock)

# The player's movement as the original's applyForces and its Box2D body decode: the walk set
# outright, the air's brake, the fall and the zero-g recoil, in the port's tick through the
# original's fixed 1/30 s physics step (planning step 148). Enhanced's player.json and the port's
# clock.json, both committed: no converted level, so no skip.
add_mp_test(test_mp_player)

# The world's time base: every dynamic body in the original's, by the one mechanism the player was carried
# by (planning step 152) - the world's gravity at SimRatio squared, a crate's fall and a stone's roll against the
# same solver at the original's own 1/30 s, a minion's and a knight's walk, the .ent materials, and Box2D's
# clamp on every body. The fall, the roll and the table are pure; the level cases skip themselves without
# the converted levels, so the suite takes no SKIPS_WITH_77.
add_mp_test(test_mp_timebase)

# What wakes a sleeper when the body it rests on goes (planning step 163, R3b-4 and R3c-5): Box2D wakes both bodies of a contact
# that is destroyed, the engine does not, and sim/Resting::Watch is the contact rule - a wall destroyed, a crate burnt, a
# door opened, a lift or platform moved, a body a portal carries off - on bare rigs stepped by the engine's own solver, and
# 3-10's knight, which falls on the tick its wall breaks. The level case skips itself without the converted levels.
add_mp_test(test_mp_wake)

# Where a picture is drawn between ticks: the tick's own place, as the original draws it, the camera
# included, and the engine's smoothing kept only where the display outruns the tick (planning step
# 150). The rules and the rate watch are pure; the walk and the level sample read the converted levels
# and skip themselves without them, so the suite takes no SKIPS_WITH_77. `test_mp test_mp_pictures
# --sweep <file>` runs all 128 levels for 420 ticks and writes what moved on the last.
add_mp_test(test_mp_pictures)

# The achievements as the original unlocks them, the saved set and its hash, and
# the Notificator's card and the pause's popup frame (planning step 143). Pure sim
# and a scratch directory; its checks against the original's list skip themselves
# without Enhanced's out/data/achievements.json, the rest run.
add_mp_test(test_mp_achievements)
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
# The draggable handle (arrows.ent) and the flame or reflector that follows it, and
# 2-20's lesson in it (planning step 139). Reads every converted level for its census.
add_mp_test(test_mp_handle SKIPS_WITH_77)
# Seesaws and spinning platforms. No tick of its own: the joint is attached when
# the level is built and the engine's solver does the rest.
add_mp_test(test_mp_hinge SKIPS_WITH_77)
# The bodies the engine steps in Box2D's order, from game/data/contact_order.json (planning step 197, CMP-18): the table's parse and its refusals,
# the flag on 2-29's stone and plank and on no other body of the 128 levels, the empty table and an unkeyed copy as the control, 2-29's stone
# against the rig clip's five readings through the layer, and the flagged stone's reading of breakable_wall_883.
add_mp_test(test_mp_contact_order SKIPS_WITH_77)
# The minions of worlds 3 and 4: spawned from a marker, walking a prefix of
# waypoints, and taken by the floor that kills them and nothing else.
add_mp_test(test_mp_minions SKIPS_WITH_77)
# What a knight sees and what it does about it (planning step 158): the cone, the three buckets of its row, the ray, the marks,
# the shot's gate, the search, the dark levels' torch rule and every patrol leg of every level. Its rig cases need no data;
# its level cases skip without it (a part of the suite, so no 77).
add_mp_test(test_mp_sight)
# What a knight does once it has seen (planning step 159): the shot's flight and its kills, the contact's hold cap, the order a
# knight's target is chosen in, a stone's crush and a barrel's blast. Its rig cases need no data and are test_mp_shoot; its level cases are the suite
# test_mp_shoot_levels of the same file (planning step 209, PORT5-7 and APPLE-22), which exits 77 without the converted levels and Enhanced's data.
add_mp_test(test_mp_shoot)
add_mp_test(test_mp_shoot_levels IN_SOURCE test_mp_shoot SKIPS_WITH_77)
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
# Where a traveller is put down under a platform (planning step 229): a capsule whose axis runs through a hull's underside is not pushed up into it. Reads 1-20 and 1-21.
add_mp_test(test_mp_arrival SKIPS_WITH_77)
# The portal shot: a tap fires, and the portal opens where the shot arrives.
add_mp_test(test_mp_shot SKIPS_WITH_77)
# Chapter 1's boss, the beholder of level31, as the original's script plays it.
add_mp_test(test_mp_boss SKIPS_WITH_77)
# A level's own art: the sprites the converter wrote, their order and blend.
# Its reader is tested on files it writes, so it runs anywhere (test_mp_sprites). The
# two parts that need the owner's data are suites of the same file (planning step 209,
# APPLE-22), each of which exits 77 without it: test_mp_sprites_original reads the
# original's extracted assets, test_mp_sprites_levels the converted levels.
add_mp_test(test_mp_sprites)
add_mp_test(test_mp_sprites_original IN_SOURCE test_mp_sprites SKIPS_WITH_77)
add_mp_test(test_mp_sprites_levels IN_SOURCE test_mp_sprites SKIPS_WITH_77)
# A level's lighting: the converter's eth_* keys, read strictly and drawn by
# nothing yet (sim/Lighting). Like test_mp_sprites, the reader is tested on files
# it writes and runs anywhere; only the part that reads the converted levels is
# skipped without them, and never with 77.
add_mp_test(test_mp_lighting)
# The lightmap of a static lit sprite, baked at one texel per bitmap unit as the original bakes it at a screen of
# 480 px or more (planning step 196, CMP-14; sim/LightmapBake). Entirely synthetic: its sprites, maps and lights are
# its own, its goldens are an independent offline model's bytes, and it reads nothing under out/ or reference/, so it
# never skips.
add_mp_test(test_mp_lightmap_bake)
# The sky the original pins to the camera: sky.json and sim/Sky's arithmetic, pure,
# and a census of what the converted levels place, which is skipped without them
# and never with 77.
add_mp_test(test_mp_sky)
# The sky on a window wider than its content, from the pictures' own pixels: reads the converted levels and their art, so it exits 77 without them.
add_mp_test(test_mp_sky_levels IN_SOURCE test_mp_sky SKIPS_WITH_77)
# The pixel-centre rule at the frame's edge: edge_cover.json and sim/EdgeCover's snap, strips and uv,
# pure arithmetic and files the suite writes: it runs anywhere.
add_mp_test(test_mp_edgecover)
# The sides of a menu picture on a wide window: side_fill.json and sim/SideFill (planning step 212), pure arithmetic; it runs anywhere.
add_mp_test(test_mp_sidefill)
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

# The five on-screen controls' geometry - sim/Controls, the one place that says where each is drawn and where a
# touch counts as on it (planning step 179) - and, from planning step 180, the MODERN controls: game/data/controls.json,
# the layout, the hit rule, the tuning and the policy of which style a run plays. Pure, reading only the port's own
# committed data (ui.json, camera.json and controls.json, from MP_TEST_PORTDATA when a mutation test sets it), so it
# never skips. Its scratch files go to the temporary directory, never to game/data.
add_mp_test(test_mp_controls)

# The modern controls' fallback pictures, sim/PadArt (planning step 180): two arrows, restart, pause, clear-portals and a gear, drawn by
# arithmetic into a bitmap in memory, the body's margin the data's. Pure; the one file it reads is the port's own controls.json (from
# MP_TEST_PORTDATA when a mutation test sets it), so it never skips.
add_mp_test(test_mp_padart)

# The Options screen and the main menu's entry to it (planning step 185): sim/Options and the `options` block of controls.json - the entry in the
# slot of the original's omitted Facebook button, the two tiles and the credits' back button at every window shape, the press, the lit tile, the
# thumbnails (the pure layout of each style, scaled), and the strict reading of the block. Pure; it reads only the port's own ui.json and
# controls.json (from MP_TEST_PORTDATA when a mutation test sets it), so it never skips.
add_mp_test(test_mp_options)

# The adjustment screen (planning step 192, CTL-11): sim/Adjust, the `adjust` block of controls.json and the editor pictures sim/PadArt draws - the seven
# widgets (a box and a face each) at every window shape, Back's fixed place under the clear-portals preview and its clearances (the numbers of
# out/parity/program/CTL/CTL11/editor_clearance.py), the tile arithmetic (limits, the size chain, re-lock, Restore), the strict-inside press, the readouts, the
# unlocked cues, the preview, the eight glyphs, and the strict reading of the block. Pure; it reads only the port's own ui.json and controls.json (from
# MP_TEST_PORTDATA when a mutation test sets it), so it never skips.
add_mp_test(test_mp_adjust)

# How a level ends: the finished and lost screens and the HUD going - ui.json's
# level_end and sim/LevelEnd's arithmetic, pinned against Enhanced's ui2 spec.
# Pure, reading only the port's own committed data, so it never skips.
add_mp_test(test_mp_levelend)

# The win cutscenes and the delay before the medal (planning step 123):
# cutscenes.json and sim/Cutscene - 4-01's launch, 4-19's landing, 2-32's knight
# and end_delay. The arithmetic reads only the port's own committed data; the
# census and the knight read the converted levels and are skipped without them,
# never with 77.
add_mp_test(test_mp_cutscene)

# The tutorial and help popups: ui.json's popups and sim/Popup's arithmetic - the
# framework, every class's waypoint loops, the close button's bounce and blink -
# pinned against Enhanced's ui2 spec, section 5. Pure, and it never skips.
add_mp_test(test_mp_popup)

# The front door: the loading screen, the main menu, and the black, press, bounce
# and blink every menu state shares - ui.json's menu_state, main_menu and loading
# and sim/MenuState, sim/MainMenu and sim/Loading, pinned against Enhanced's ui3
# spec, sections 0 and 2. Pure, reading only the port's own committed data, so it
# never skips.
add_mp_test(test_mp_frontdoor)

# Credits and the achievements dashboard, and what the save has opened: ui.json's
# credits, dashboard and locking with sim/Credits, sim/Dashboard, sim/Achievements and
# sim/Locking, pinned against Enhanced's ui3 spec, sections 3 and 4. Pure, and it
# reads only the port's own committed data - its achievement lists are its own, never
# the original's - so it never skips.
add_mp_test(test_mp_info)
add_mp_test(test_mp_select)

# The layer, attached to a bare registry: no window, no Vulkan. It drives taps
# through the camera, and plays level30 from the spawn to the exit.
add_mp_test(test_mp_layer SKIPS_WITH_77)
# The classic goldens, tests/golden/*.json, are read at run time and found through this one source's definition, as the routes'
# directory is: a corrected golden is a file, never a relink. `test_mp test_mp_layer --record-golden <dir>` writes them on the
# unmodified claim code and refuses to overwrite a file (planning step 178).
set_property(SOURCE test_mp_layer.cpp APPEND PROPERTY
             COMPILE_DEFINITIONS "MAGICPORTALS_GOLDEN_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/golden\"")

# The layer speaks the language (planning step 238, PORT-34). Its first half is a snapshot guard: the quad lists of every menu screen, the
# level texts and the cards, kept as numbers in tests/golden/menu_screens.json and compared within a tolerance (dumped from the wired layer in the very run
# that proved it equal to the bit-hash golden recorded BEFORE the languages were wired in; `test_mp test_mp_langlayer --record-golden <dir>` writes it and
# refuses to overwrite), so that English is proven to draw what it always drew.
# Its second half drives each language through the layer. Reads the converted levels, Enhanced's achievements list and the original's pictures,
# and skips with 77 without the levels (the cases that need the rest say so and skip themselves).
add_mp_test(test_mp_langlayer SKIPS_WITH_77)
set_property(SOURCE test_mp_langlayer.cpp APPEND PROPERTY
             COMPILE_DEFINITIONS "MAGICPORTALS_GOLDEN_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/golden\"")

# The modules that speak a language, without a layer (planning step 238): the fit to a slot's width over a font baked at run time, what each module
# takes from the string table, the hint's mirroring, the optional limits' reading (and its refusals), and the achievements' translation overlay.
# Every text is synthetic, and the suite needs nothing outside this repository.
add_mp_test(test_mp_langsim)

# Does every translation fit its box? (planning step 238) The fourteen interface strings in the real font set of each of the eleven languages against
# the limits the rule tables name, and - a suite of its own that exits 77 without the gitignored achievements.<code>.json files - the achievements'
# titles, descriptions, unlock cards and popups, and the locked secret achievements' words.
add_mp_test(test_mp_overflow)
add_mp_test(test_mp_overflow_achievements IN_SOURCE test_mp_overflow SKIPS_WITH_77)

# The language picker (planning step 239, PORT-35). test_mp_picker is the pure half: picker.json as it is and as its loader refuses it, the grid and the two
# globes at the nine shapes of window (clear of the back button, of each other and of every button they share a screen with), the touch rule, the pieces, and
# the atlas of the eleven names baked from the committed faces. test_mp_pickerlayer drives the layer: the globe, the screen, every name drawn without a hole
# in every language, a choice kept and read back by a fresh layer, and the runs that must have no picker. It needs the converted levels and the original's
# pictures, and exits 77 without them.
add_mp_test(test_mp_picker)
add_mp_test(test_mp_pickerlayer IN_SOURCE test_mp_picker SKIPS_WITH_77)

# Every level a win route plays from its start to its door (game/Route.hpp): the
# routes are tests/routes/<level>.json, found at run time, so a route is added
# without touching the suite. Played through the layer on the DEV schedule main's
# --route uses. Route.cpp is compiled into test_mp (above) and, when
# MAGICPORTALS_DEV_TOOLS is on, into MagicPortals rather than put in
# MagicPortalsGame; that kept the other suites from relinking
# when it changed, which one executable no longer needs, and moving it would
# relink the game for nothing (step 129 left it). The routes' directory is this
# one source file's definition, so moving it recompiles one suite.
add_mp_test(test_mp_routes SKIPS_WITH_77)
# The controls census (`test_mp test_mp_routes --controls modern`, planning step 182) reads the accepted list of tests/golden/controls_census.json for
# its lower bound, at run time and through this one source's definition, as test_mp_census does: a corrected golden is a file, never a relink.
set_property(SOURCE test_mp_routes.cpp APPEND PROPERTY
             COMPILE_DEFINITIONS "MAGICPORTALS_ROUTES_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/routes\""
                                 "MAGICPORTALS_GOLDEN_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/golden\"")

# What the modern controls' default layout costs the levels (planning step 180, design 1.9): of the 128 recorded winning routes, which levels
# have a shot no camera can bring clear of a control's hit rectangle. Pure geometry over tests/routes/*.json and the converted levels, so it
# skips with 77 without them. The accepted list and the design's tables are data, tests/golden/controls_census.json, found through this one
# source's definitions as the routes' and the classic goldens' directories are: a corrected golden is a file, never a relink. The list is
# valid only for the hit growth that ships (0), and the suite fails the moment controls.json says otherwise.
add_mp_test(test_mp_census SKIPS_WITH_77)
set_property(SOURCE test_mp_census.cpp APPEND PROPERTY
             COMPILE_DEFINITIONS "MAGICPORTALS_ROUTES_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/routes\""
                                 "MAGICPORTALS_GOLDEN_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/golden\"")

# And the static check beside it: no level's door sealed off in the port, rerun over the
# converted levels this build reads and compared with tools/parity/reachability.json. It is
# Python, not a suite, so it is registered only where the parity venv is (README, Parity work);
# it exits 77 - SKIPPED - without the converted levels, as the suites above do.
# The venv's interpreter lives in Scripts/ on Windows and bin/ elsewhere; reachability.py needs only the standard library, so a build that has no
# venv can name any python3 here (tools/build_linux.sh does).
if (WIN32)
    set(_parityVenvPython "${CMAKE_SOURCE_DIR}/tools/parity/.venv/Scripts/python.exe")
else()
    set(_parityVenvPython "${CMAKE_SOURCE_DIR}/tools/parity/.venv/bin/python")
endif()
set(SUPERSONIC_MAGICPORTALS_PARITY_PYTHON "${_parityVenvPython}"
    CACHE FILEPATH "The parity venv's Python, which runs the reachability check")
if (EXISTS "${SUPERSONIC_MAGICPORTALS_PARITY_PYTHON}")
    add_test(NAME parity_reachability
             COMMAND "${SUPERSONIC_MAGICPORTALS_PARITY_PYTHON}" "${CMAKE_SOURCE_DIR}/tools/parity/reachability.py"
                     --check --levels-dir "${SUPERSONIC_MAGICPORTALS_LEVELS}"
                     --chapters "${SUPERSONIC_MAGICPORTALS_CHAPTERS}")
    set_tests_properties(parity_reachability PROPERTIES SKIP_RETURN_CODE 77)

    # The two regression pins the build lacked (planning step 208, PORT-10). Python, like the check above, so they are registered where it is: they are not suites of
    # test_mp, they run the build's own tools on scratch copies of two files and are never linked into anything.
    #   test_mp_build_dev_tools_option  a configure with MAGICPORTALS_DEV_TOOLS set to anything but AUTO, ON or OFF fails with the FATAL_ERROR of the top-level
    #                                   CMakeLists.txt, and the three words get past that line (CMake, a C and a C++ compiler)
    #   test_mp_build_profile_header    a translation unit that includes game/BuildProfile.hpp without the build constants fails to compile with the header's #error,
    #                                   and compiles with them (-fsyntax-only: no object, no link)
    # Each asserts on the exact message read out of the source, carries positive controls and runs its named mutations on temporary copies; each exits 77 (SKIPPED, never
    # a pass) where it cannot run: on Windows (a configure there builds a compiler-identification program, and no Windows link is made for a pin), with no cmake or
    # compiler, or with MSVC. They are run in the Linux tree.
    function(add_build_pin name script)
        add_test(NAME ${name}
                 COMMAND "${SUPERSONIC_MAGICPORTALS_PARITY_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/build_pins/${script}"
                         --repo "${CMAKE_SOURCE_DIR}" --cmake "${CMAKE_COMMAND}"
                         --c-compiler "${CMAKE_C_COMPILER}" --cxx-compiler "${CMAKE_CXX_COMPILER}" --cxx-id "${CMAKE_CXX_COMPILER_ID}")
        set_tests_properties(${name} PROPERTIES SKIP_RETURN_CODE 77)
    endfunction()
    add_build_pin(test_mp_build_dev_tools_option dev_tools_option.py)
    add_build_pin(test_mp_build_profile_header build_profile_header.py)
endif()
