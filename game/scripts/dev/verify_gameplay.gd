extends Node2D
## End-to-end gameplay test over all 32 chapter-1 levels.
##
## Run:
##     tools\godot.bat --headless --path game res://scenes/dev/verify_gameplay.tscn
##
## This is the strongest single check in the project, because it closes the loop from raw
## `.esc` bytes to working physics. **If the collision polygons converted wrongly, the
## player falls through the floor and this fails.** No amount of node-counting proves
## that; only running the simulation does.
##
## Per level it asserts: a spawn marker exists, role classification found the expected
## entities, static portals pair to real partners, and — the load-bearing one — a player
## dropped at the spawn point comes to rest on geometry instead of falling out of the
## world.
##
## Exits non-zero on failure. Needs converted levels; without them it skips and exits 0.

## Physics frames to simulate per level. Spawns sit on or just above ground, so a landing
## resolves in a handful of frames; 45 (0.75 s at 60 Hz) is ample and keeps a 128-level
## sweep inside a sane runtime. The loop breaks the moment the body lands, so this is a
## ceiling rather than a cost paid per level.
const SETTLE_FRAMES: int = 45

## How far below the level's own bounds counts as "fell out of the world". Generous, so a
## legitimately deep level is never reported as a fall.
const FELL_OUT_MARGIN_PX: float = 2000.0

## Vertical movement allowed for a zero-g spawn. Half a tile: enough for collision
## depenetration, far below the ~340 px gravity would produce over SETTLE_FRAMES.
const ZERO_G_TOLERANCE_PX: float = 16.0

var _passed: int = 0
var _failed: int = 0
var _fell_through: Array[String] = []
var _no_ground: Array[String] = []
var _movers: int = 0
var _switches: int = 0
var _doors: int = 0
var _inert_animated: int = 0
var _hazards: Dictionary = {}


func _ready() -> void:
	if not DataLoader.is_ready:
		print("SKIP — %s" % DataLoader.missing_reason)
		get_tree().quit(0)
		return
	GameState.reset_progress()
	call_deferred("_run")


func _check(label: String, condition: bool, detail: String = "") -> void:
	if condition:
		_passed += 1
	else:
		_failed += 1
		printerr("FAIL  %s%s" % [label, "" if detail.is_empty() else "  — " + detail])


func _run() -> void:
	var runtime: LevelRuntime = LevelRuntime.new()
	add_child(runtime)
	var portals: PortalSystem = PortalSystem.new()
	add_child(portals)

	var total_crystals: int = 0
	var total_portals: int = 0
	var levels: int = DataLoader.level_count(0)
	_check("chapter 1 has levels", levels == 32, "got %d" % levels)

	# Every converted world, not just chapter 1 — the physics/landing assertion is the
	# only thing proving the collision conversion is right, and it should cover all of it.
	var swept: int = 0
	# Flattened to a single loop rather than nesting, so the body keeps one indent level —
	# a nested loop here silently produced a tab/space mix that Godot rejects outright.
	var targets: Array[Vector2i] = []
	for world: int in range(4):
		for level: int in range(DataLoader.level_count(world)):
			targets.append(Vector2i(world, level))

	for target: Vector2i in targets:
		var world: int = target.x
		var level: int = target.y
		swept += 1
		var path: String = DataLoader.level_scene_path(world, level)
		var packed: PackedScene = load(path) as PackedScene
		if packed == null:
			_check("level %d loads" % (level + 1), false, path)
			continue

		var root: Node = packed.instantiate()
		add_child(root)
		runtime.bind(root)
		portals.bind(runtime)

		var tag: String = "%d-%d" % [world + 1, level + 1]
		_check("%s has a spawn" % tag, runtime.spawn_position != Vector2.ZERO
			or not runtime.nodes_with_role(&"player_spawn").is_empty())
		_check("%s has an exit door" % tag, not runtime.nodes_with_role(&"exit_door").is_empty())
		_check("%s has level bounds" % tag, runtime.bounds_max != Vector2.ZERO)

		total_crystals += runtime.crystals_total
		var portal_nodes: Array = runtime.nodes_with_role(&"static_portal")
		total_portals += portal_nodes.size()

		# A static portal's `destiny` must name either another static portal, or a slot the
		# player is allowed to fill.
		#
		# THE INDEX SPACE IS SHARED between static and player-placed portals — discovered
		# here, by this assertion failing. Level 1-2 ships ONE static portal, index 0,
		# destiny 1, and `max_portals` 1: the destination is the portal the PLAYER places.
		# That is how the game teaches placement. So an unresolved destiny is legitimate
		# exactly when the level grants a portal budget, and a bug otherwise.
		for node: Node2D in portal_nodes:
			if not node.has_meta("destiny") or not node.has_meta("index"):
				continue
			var destiny: int = int(String(node.get_meta("destiny")))
			var found: bool = false
			for other: Node2D in portal_nodes:
				if other.has_meta("index") and int(String(other.get_meta("index"))) == destiny:
					found = true
					break
			_check(
				"%s portal destiny %d is static or player-placeable" % [tag, destiny],
				found or runtime.max_portals > 0,
				"no static portal has index %d and the level grants no portal budget" % destiny
			)

		_count_behaviours(root, runtime)
		await _drop_player(root, runtime, tag)
		root.free()
	_check("swept every converted level", swept == 128, "got %d" % swept)
	_check("moving platforms and lifts got movers", _movers > 0, "got %d" % _movers)
	_check("buttons got switch behaviours", _switches > 0, "got %d" % _switches)
	_check("switched doors got movers", _doors > 0, "got %d" % _doors)
	# The authoritative test for "does this entity DO anything" is whether the original's
	# binary has a matching ETHCallback_<name>. These three have none, so animating them
	# would invent gameplay the original never had.
	_check("INERT entities stayed inert", _inert_animated == 0,
		"%d inert entities were animated" % _inert_animated)

	# The destruction economy. Counts are measured from the level data, so a drop here
	# means the role table and the corpus have drifted apart.
	_check("rolling stones demolish", int(_hazards.get(&"demolisher", 0)) == 27,
		"got %s" % _hazards.get(&"demolisher", 0))
	_check("barrel bombs are explosive", int(_hazards.get(&"explosive", 0)) == 31,
		"got %s" % _hazards.get(&"explosive", 0))
	_check("shock fields exist", int(_hazards.get(&"shock_field", 0)) > 0)
	_check("gravity wells exist (chapter 4)", int(_hazards.get(&"gravity_well", 0)) > 0)
	_check("carranca turrets exist", int(_hazards.get(&"turret", 0)) > 0)
	# Exact, not "> 0". Measured from the corpus: 65 instances carry burnable=1, and one
	# carries burnable="0" which must NOT count — 0 is a real "off" value, not absence.
	# An earlier version applied flags only to role-classified nodes and got 11 here;
	# crates have no gameplay role, so most of the game's burnables were silently skipped.
	_check("all 65 burnable objects are flagged", int(_hazards.get("burnable", 0)) == 65,
		"got %s" % _hazards.get("burnable", 0))

	# Keys, locks and enemies — worlds 3-4 only, so these also assert that chapter 1 does
	# NOT quietly acquire them.
	_check("keys are colour-paired", int(_hazards.get(&"key", 0)) == 39,
		"got %s" % _hazards.get(&"key", 0))
	_check("keyholes match keys", int(_hazards.get(&"keyhole", 0)) == 41,
		"got %s" % _hazards.get(&"keyhole", 0))
	_check("locked doors match keyholes", int(_hazards.get(&"locked_door", 0)) == 41,
		"got %s" % _hazards.get(&"locked_door", 0))
	_check("minion spawners exist", int(_hazards.get(&"enemy_spawn", 0)) == 62,
		"got %s" % _hazards.get(&"enemy_spawn", 0))
	# One boss marker per chapter, always in level 32 - plus dragon_knight_spawn, which
	# shares 2-32 with the dragon itself.
	_check("every chapter has a boss", int(_hazards.get(&"boss_spawn", 0)) == 5,
		"got %s" % _hazards.get(&"boss_spawn", 0))
	_check("hinges pivot", int(_hazards.get(&"hinge", 0)) == 10,
		"got %s" % _hazards.get(&"hinge", 0))

	await _test_portal_mechanic()

	print("--------------------------------------------------")
	print("levels        : %d" % swept)
	print("crystals      : %d" % total_crystals)
	print("static portals: %d" % total_portals)
	print("behaviours    : %d movers, %d switches, %d doors" % [_movers, _switches, _doors])
	print("hazards       : %s" % _hazards)
	if not _fell_through.is_empty():
		printerr("FELL THROUGH GEOMETRY: %s" % ", ".join(_fell_through))
	if not _no_ground.is_empty():
		print("never landed (may be legitimate — no floor under spawn): %s" % ", ".join(_no_ground))
	print("passed: %d   failed: %d" % [_passed, _failed])
	GameState.reset_progress()
	get_tree().quit(1 if _failed > 0 else 0)


func _drop_player(root: Node, runtime: LevelRuntime, tag: String) -> void:
	var player: Player = Player.new()
	player.position = runtime.spawn_position
	# Mirror what Main does. Chapter 4 levels 2-19 set `no_gravity` — that is the space
	# chapter, and a player that falls there falls out of every level.
	var zero_g: bool = runtime.flag("no_gravity")
	player.gravity_enabled = not zero_g
	root.add_child(player)

	var start_y: float = player.position.y
	var floor_limit: float = maxf(runtime.bounds_max.y, start_y) + FELL_OUT_MARGIN_PX
	var landed: bool = false

	for _frame: int in range(SETTLE_FRAMES):
		await get_tree().physics_frame
		if not is_instance_valid(player):
			break
		if player.is_on_floor():
			landed = true
			break
		if player.position.y > floor_limit:
			break

	if is_instance_valid(player):
		if zero_g:
			# In zero-g the expectation inverts: the player must not FALL. A few pixels of
			# movement is legitimate collision depenetration — a spawn overlapping geometry
			# gets pushed clear by move_and_slide — so the tolerance is half a tile.
			#
			# That still discriminates sharply: gravity over this window would produce
			# ~340 px of fall, twenty times the threshold. This is not a "wide enough to
			# always pass" bound, it is a gap between two well-separated magnitudes.
			_check("%s zero-g player does not fall" % tag,
				absf(player.position.y - start_y) < ZERO_G_TOLERANCE_PX,
				"moved %.1f px" % (player.position.y - start_y))
			player.free()
			return
		var fell: bool = player.position.y > floor_limit
		# THE assertion: the player must come to REST on geometry.
		#
		# Asserting `not fell` instead is decoration, and this was verified by mutation:
		# with every collider stripped from the converter output, all 32 levels reported
		# "never landed" and the suite still passed 136/136. The fall limit sits ~2256 px
		# below spawn, but 120 frames at the guessed gravity covers only ~1500 px — so a
		# player falling through the entire world never reaches the threshold and the test
		# passes by TIMING OUT. `landed` has no such escape.
		#
		# Safe to require of every level: on correct output `_no_ground` is empty across
		# all 32. If a genuinely floorless spawn is ever added, enumerate it here as a
		# named exception rather than weakening this back to `not fell`.
		_check("%s player lands on converted geometry" % tag, landed,
			"y went from %.0f to %.0f after %d frames (fell out: %s)"
			% [start_y, player.position.y, SETTLE_FRAMES, fell])
		if fell:
			_fell_through.append(tag)
		elif not landed:
			_no_ground.append(tag)
		player.free()


## The portal loop, per OWNER testimony (docs/original-gameplay.md §2.0):
## at most two placed portals, and traversing destroys the pair.
func _test_portal_mechanic() -> void:
	# Level 1-3 grants the default budget (it places no `properties` entity), which is the
	# normal case — 26 of 32 chapter-1 levels.
	var packed: PackedScene = load(DataLoader.level_scene_path(0, 2)) as PackedScene
	var root: Node = packed.instantiate()
	add_child(root)

	var runtime: LevelRuntime = LevelRuntime.new()
	add_child(runtime)
	var portals: PortalSystem = PortalSystem.new()
	add_child(portals)
	runtime.bind(root)
	portals.bind(runtime)

	_check("default budget is two portals", runtime.max_portals == 2,
		"got %d" % runtime.max_portals)

	var spawn: Vector2 = runtime.spawn_position
	_check("first portal places", portals.try_place(spawn + Vector2(0, -80)))
	_check("second portal places", portals.try_place(spawn + Vector2(120, -80)))
	_check("two portals are live", portals.get_placed_count() == 2)

	# At the cap the OLDEST is recycled rather than the placement being refused — a GUESS,
	# pinned here so changing it is a deliberate act.
	_check("third placement recycles rather than refusing",
		portals.try_place(spawn + Vector2(240, -80)))
	_check("still only two live", portals.get_placed_count() == 2,
		"got %d" % portals.get_placed_count())
	# The running total keeps climbing — that is what the golden score measures.
	_check("total created keeps counting", portals.portals_used == 3,
		"got %d" % portals.portals_used)

	# Traversal consumes the pair. Drive it directly rather than waiting for physics to
	# push a body through a trigger, which would make the test slow and flaky.
	var player: Player = Player.new()
	player.position = spawn
	root.add_child(player)
	portals.set_player(player)
	await get_tree().physics_frame

	var before: int = portals.get_placed_count()
	portals.force_traverse_for_test(player)
	await get_tree().physics_frame
	_check("traversing destroyed the pair", portals.get_placed_count() == 0,
		"was %d, now %d" % [before, portals.get_placed_count()])

	# A level that explicitly stores 0 grants no placement at all.
	var l1: Node = (load(DataLoader.level_scene_path(0, 0)) as PackedScene).instantiate()
	add_child(l1)
	runtime.bind(l1)
	portals.bind(runtime)
	_check("level 1-1 grants no placement", runtime.max_portals == 0)
	_check("placement refused with zero budget", not portals.try_place(Vector2(100, 100)))
	l1.free()

	runtime.free()
	portals.free()
	root.free()


func _count_behaviours(root: Node, runtime: LevelRuntime) -> void:
	for role: StringName in [&"moving_platform", &"lift"]:
		for node: Node2D in runtime.nodes_with_role(role):
			if node.get_node_or_null("Behaviour") != null:
				_movers += 1
	for node: Node2D in runtime.nodes_with_role(&"switch"):
		if node.get_node_or_null("Behaviour") != null:
			_switches += 1
	for node: Node2D in runtime.nodes_with_role(&"switched_door"):
		if node.get_node_or_null("Behaviour") != null:
			_doors += 1
	for role: StringName in [&"demolisher", &"explosive", &"fire_source", &"shock_field",
			&"gravity_well", &"turret", &"launcher", &"key", &"keyhole", &"locked_door",
			&"enemy_spawn", &"hinge", &"boss_spawn"]:
		for node: Node2D in runtime.nodes_with_role(role):
			if (node.get_node_or_null("Hazard") != null
					or node.get_node_or_null("Actor") != null
					or node.get_node_or_null("Behaviour") != null):
				_hazards[role] = int(_hazards.get(role, 0)) + 1
			elif role in [&"locked_door"]:
				_hazards[role] = int(_hazards.get(role, 0)) + 1
	for child: Node in root.get_children():
		var node: Node2D = child as Node2D
		if node == null or not node.has_meta("entity_name"):
			continue
		if node.get_node_or_null("Burnable") != null:
			_hazards["burnable"] = int(_hazards.get("burnable", 0)) + 1
		if String(node.get_meta("entity_name")) in ["moving_platform_single.ent", "platform", "anchor"]:
			if node.get_node_or_null("Behaviour") != null:
				_inert_animated += 1
