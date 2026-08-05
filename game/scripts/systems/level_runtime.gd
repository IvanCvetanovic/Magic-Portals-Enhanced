class_name LevelRuntime
extends Node
## Turns a converted level into a playable one.
##
## The converter deliberately emits dumb geometry: every entity is a `Node2D` carrying
## `metadata/entity_name` plus its original custom data, and nothing more. Deciding that
## `door.ent` is an exit is a game-design call, so it happens here, driven by
## `res://data/entity_roles.json` (CLAUDE.md rule 1 — a designer retunes JSON, not code).
##
## This also honours CLAUDE.md rule 5: one generic treatment plus data, not a scene per
## entity type. Roles are attached to the converted nodes in place.
##
## PERFORMANCE (rule 6): role classification is a ONE-TIME scan at level load, and
## triggers are Area2D signals. Nothing scans for targets per frame.

## Roles that need an overlap trigger built for them. The original gives these entities
## `shape=BS_NONE` — no physics fixture — but still ships a `<Collision>` box and tests
## overlap itself, so the converter preserves that box as `metadata/trigger_size`.
const TRIGGER_ROLES: Array[StringName] = [
	&"exit_door", &"collectible", &"hazard", &"static_portal", &"no_portal_zone",
	&"switch", &"reflector"
]

## Roles whose node must be able to MOVE and carry riders. A teleported StaticBody2D does
## not carry anything standing on it — the player would slide off — so these are converted
## to AnimatableBody2D with sync_to_physics.
const MOVING_ROLES: Array[StringName] = [&"moving_platform", &"lift", &"switched_door"]

## Fallback trigger box for a role entity whose collision data did not survive. Chosen to
## be small enough not to create phantom pickups; a warning is pushed when it is used.
const FALLBACK_TRIGGER_PX: Vector2 = Vector2(16, 16)

signal player_spawned(player: Node2D)
signal crystal_collected(remaining: int)
signal exit_reached
signal player_died(reason: StringName)

var spawn_position: Vector2 = Vector2.ZERO
var camera_start: Vector2 = Vector2.ZERO
var bounds_max: Vector2 = Vector2.ZERO
var max_portals: int = 0

## Every key on the level's `properties` entity, verbatim. Values are strings because that
## is how the original stores them. Chapter 4 is where these matter most: levels 4-2..4-19
## set `no_gravity`, and 4-20..4-32 set `darkest` (the "Alone in the dark" achievements).
var properties: Dictionary = {}
var crystals_total: int = 0
var crystals_remaining: int = 0

## The instantiated level. Anything spawned during play (placed portals, effects) parents
## here rather than to the game root, so a retry frees it along with the level and cannot
## leak state into the next attempt.
var level_root: Node = null

var _by_role: Dictionary = {}
var _name_to_role: Dictionary = {}


func _ready() -> void:
	_build_role_lookup()


func _build_role_lookup() -> void:
	# Flatten {role: {names: [...]}} into {name: role} once, so classification is a dict
	# hit per node instead of a scan over every role's name list.
	var roles: Variant = DataLoader.get_value("entity_roles", ["roles"], {})
	if not (roles is Dictionary):
		push_warning("LevelRuntime: entity_roles.json has no 'roles' object")
		return
	for role: String in (roles as Dictionary):
		var entry: Variant = (roles as Dictionary)[role]
		if not (entry is Dictionary):
			continue
		var names: Variant = (entry as Dictionary).get("names", [])
		if not (names is Array):
			continue
		for entity_name: String in (names as Array):
			_name_to_role[entity_name] = StringName(role)


## Scan a freshly instantiated level and wire up everything with a gameplay role.
func bind(root: Node) -> void:
	level_root = root
	_by_role.clear()
	spawn_position = Vector2.ZERO
	camera_start = Vector2.ZERO
	bounds_max = Vector2.ZERO
	max_portals = 0
	crystals_total = 0

	for child: Node in root.get_children():
		var node: Node2D = child as Node2D
		if node == null or not node.has_meta("entity_name"):
			continue
		var role: Variant = _name_to_role.get(String(node.get_meta("entity_name")))
		if role == null:
			continue  # scenery: renders and collides, no behaviour
		var key: StringName = role
		if not _by_role.has(key):
			_by_role[key] = [] as Array[Node2D]
		(_by_role[key] as Array).append(node)

	_read_markers()
	for role: StringName in TRIGGER_ROLES:
		for node: Node2D in nodes_with_role(role):
			_attach_trigger(node, role)

	for role: StringName in MOVING_ROLES:
		for node: Node2D in nodes_with_role(role):
			_make_movable(node)

	# Behaviours attach AFTER triggers, because several of them (switch, reflector) bind
	# to the Trigger node the step above creates.
	#
	# Roles go to the role-classified nodes; FLAGS go to EVERY entity. That split matters:
	# a crate has no gameplay role — it is a physics prop — but it still carries
	# `burnable`, and an earlier version only ran the flag pass over role-classified nodes,
	# so 11 of the game's burnable objects were flagged and the rest silently were not.
	for child: Node in root.get_children():
		var node: Node2D = child as Node2D
		if node == null or not node.has_meta("entity_name"):
			continue
		var role: Variant = _name_to_role.get(String(node.get_meta("entity_name")))
		var key: StringName = role if role != null else &""
		if role != null:
			Behaviours.attach(node, key, self)
			Actors.attach(node, key, self)
		# Hazards runs for EVERY entity, role or not: it also applies the per-instance
		# flags (burnable, blink), and a crate carries those while having no role at all.
		Hazards.attach(node, key, self)

	_wire_channels()
	_apply_gravity_mode(root)
	crystals_remaining = crystals_total


## Position of a named marker entity, or `fallback` when the level does not place one.
##
## The original references markers BY NAME from custom data — a lift's `a`/`b` name
## `liftA`/`liftB`. Resolved here rather than in the converter so the level data keeps the
## original's own vocabulary and the mapping stays inspectable.
func marker_named(entity_name: String) -> Node2D:
	"""Any entity placed under that exact EntityName, whatever its role.

	Waypoints and lift endpoints are both referenced by name from custom data, so this is
	the one lookup the runtime genuinely needs to do by string.
	"""
	if entity_name.is_empty():
		return null
	for role: StringName in _by_role:
		for node: Node2D in nodes_with_role(role):
			if String(node.get_meta("entity_name", "")) == entity_name:
				return node
	return null


func marker_position(entity_name: String, fallback: Vector2) -> Vector2:
	if entity_name.is_empty():
		return fallback
	for node: Node2D in nodes_with_role(&"lift_marker"):
		if String(node.get_meta("entity_name", "")) == entity_name:
			return node.position
	return fallback


func _make_movable(node: Node2D) -> void:
	# The converter emits a static Body child. Re-typing it in place is not possible, so
	# the collision shapes are re-parented onto an AnimatableBody2D instead.
	var body: Node = node.get_node_or_null("Body")
	if body == null or body is AnimatableBody2D:
		return
	var animatable: AnimatableBody2D = AnimatableBody2D.new()
	animatable.name = "MovingBody"
	# Without this the body teleports and riders are not carried along with it.
	animatable.sync_to_physics = true
	for shape: Node in body.get_children():
		body.remove_child(shape)
		animatable.add_child(shape)
	node.remove_child(body)
	body.free()
	node.add_child(animatable)


func _wire_channels() -> void:
	# button.idx and door_lift.switchIdx are the same numbering. Doors listen; buttons
	# publish. Routed through EventBus so neither has to find the other.
	var doors: Array = nodes_with_role(&"switched_door")
	if doors.is_empty():
		return
	EventBus.channel_changed.connect(
		func(channel: int, pressed: bool) -> void:
			for door: Node2D in doors:
				if not is_instance_valid(door):
					continue
				if int(String(door.get_meta("switchIdx", "-1"))) != channel:
					continue
				var mover: Node = door.get_node_or_null("Behaviour")
				if mover != null and mover.has_method("set_open"):
					mover.set_open(pressed)
	)


func nodes_with_role(role: StringName) -> Array:
	return _by_role.get(role, [])


func _read_markers() -> void:
	var spawns: Array = nodes_with_role(&"player_spawn")
	if spawns.is_empty():
		push_warning("LevelRuntime: level has no player_spawn (main_char)")
	else:
		spawn_position = (spawns[0] as Node2D).position

	var cameras: Array = nodes_with_role(&"camera_start")
	camera_start = (cameras[0] as Node2D).position if not cameras.is_empty() else spawn_position

	var bounds: Array = nodes_with_role(&"level_bounds")
	if not bounds.is_empty():
		bounds_max = (bounds[0] as Node2D).position

	# `properties` is placed in only 6 of 32 chapter-1 scenes. Absent means "use the
	# default", NOT "zero portals" — level 1-1 explicitly stores max_portals=0, so the
	# absence and the explicit zero are different states and must not be conflated.
	var props: Array = nodes_with_role(&"level_properties")
	max_portals = _default_max_portals()
	properties = {}
	if not props.is_empty():
		var node: Node2D = props[0]
		for entry: Dictionary in node.get_meta_list().map(func(k: String) -> Dictionary:
			return {"k": k, "v": node.get_meta(k)}):
			if entry["k"] != "entity_name":
				properties[entry["k"]] = entry["v"]
		if node.has_meta("max_portals"):
			max_portals = int(String(node.get_meta("max_portals")))

	crystals_total = nodes_with_role(&"collectible").size()


func _default_max_portals() -> int:
	return int(DataLoader.get_value("portals", ["placement", "default_max_portals"], 2))


func _attach_trigger(node: Node2D, role: StringName) -> void:
	var size: Vector2 = FALLBACK_TRIGGER_PX
	if node.has_meta("trigger_size"):
		size = node.get_meta("trigger_size")
	else:
		push_warning(
			"LevelRuntime: %s has no trigger_size; using fallback" % node.get_meta("entity_name")
		)

	var area: Area2D = Area2D.new()
	area.name = "Trigger"
	# Detection only — these must never push the player around. The original's role
	# entities have no physics fixture at all (shape=BS_NONE).
	area.monitorable = false
	if node.has_meta("trigger_offset"):
		area.position = node.get_meta("trigger_offset")

	var shape: CollisionShape2D = CollisionShape2D.new()
	var rect: RectangleShape2D = RectangleShape2D.new()
	rect.size = size
	shape.shape = rect
	area.add_child(shape)
	node.add_child(area)

	area.body_entered.connect(_on_trigger_entered.bind(node, role))


func _on_trigger_entered(body: Node2D, node: Node2D, role: StringName) -> void:
	# Only the player triggers anything. Crates and stones are pushed around by physics
	# and would otherwise collect crystals and finish levels on the player's behalf.
	if not body.is_in_group("player"):
		return
	match role:
		&"collectible":
			_collect(node)
		&"exit_door":
			exit_reached.emit()
		&"hazard":
			player_died.emit(&"hazard")


func _collect(node: Node2D) -> void:
	# Guard against a second trigger firing before the node is gone — Area2D signals can
	# arrive more than once for one physical overlap.
	if not is_instance_valid(node) or node.is_queued_for_deletion():
		return
	crystals_remaining = maxi(0, crystals_remaining - 1)
	node.queue_free()
	crystal_collected.emit(crystals_remaining)


## True when the exit should let the player through.
##
## UNVERIFIED: whether the original gates its exit on collecting every crystal is not
## established — no identifier in the binary settles it, and it is not observable without
## playing. Modelled as a data switch rather than a hardcoded rule so flipping it later is
## a JSON edit. See `portals.json` → `level.exit_requires_all_crystals`.
func exit_is_open() -> bool:
	var requires: bool = bool(
		DataLoader.get_value("portals", ["level", "exit_requires_all_crystals"], false)
	)
	return not requires or crystals_remaining == 0


## True when a level property flag is set. Absent means false.
##
## The engine stores these as strings; "0" is a real value meaning off (chapter 4 sets
## `door_emissive` to "0"), so a bare presence check would be wrong.
func flag(key: String) -> bool:
	if not properties.has(key):
		return false
	var raw: String = String(properties[key])
	return raw != "" and raw != "0"


## A numeric level property, or `fallback` when absent or unparseable.
func number(key: String, fallback: float) -> float:
	if not properties.has(key):
		return fallback
	return String(properties[key]).to_float()


func _apply_gravity_mode(root: Node) -> void:
	## `no_gravity` is a LEVEL property, not a player one.
	##
	## Found by a test failure on 4-9: the player was zero-g but the crates were not, so a
	## `crate_low_emissive` fell under Godot's project gravity and shoved the player 142 px
	## down. Every dynamic body in the level has to float, not just the character — which
	## is also what makes these levels puzzles about pushing objects through portals.
	if not flag("no_gravity"):
		return
	var stack: Array[Node] = [root]
	while not stack.is_empty():
		var current: Node = stack.pop_back()
		var rigid: RigidBody2D = current as RigidBody2D
		if rigid != null:
			rigid.gravity_scale = 0.0
		for child: Node in current.get_children():
			stack.push_back(child)
