class_name Actors
extends Node
## Keys and locks, patrolling minions, and the markers that drive them.
##
## Worlds 3 and 4 only — chapter 1 places none of this. **No enemy is ever placed directly
## in a scene**: every minion, boss and projectile is script-spawned from an invisible
## marker entity, which is why the level data contains `minion_spawn` markers and no
## `minion` instances.
##
## PATROL. A spawner's `waypointName` is a **PREFIX**, not a name: `wayA` means the minion
## patrols between the marker entities `wayA0` and `wayA1`, holding at each for that
## marker's own `holdTime`. `holdTime` is in MILLISECONDS. A lone `wayX0` carrying a
## sentinel hold of 9999999 is how the designers authored a stationary guard — so a
## missing partner marker is deliberate, not missing data.
##
## MINIONS ARE PUZZLE OBJECTS, NOT JUST THREATS. They carry `breakable`, `burnable` and
## `teleportable` all set, so they can be crushed by a rolling stone, burned, or portalled
## off the level onto `enemy_killer` — an invisible solid kill floor placed below the
## bounds of every world-3 level. That is what the "Kill all minions" achievements are for.
##
## Every timing, speed and range here is a marked GUESS in `res://data/actors.json`. The
## structure is DATA; the numbers are not.


static func attach(node: Node2D, role: StringName, runtime: LevelRuntime) -> void:
	match role:
		&"key":
			_add(node, KeyItem.new())
		&"keyhole":
			_add(node, Keyhole.new(runtime))
		&"enemy_spawn":
			_add(node, Spawner.new(runtime))
		_:
			pass


static func _add(node: Node2D, behaviour: Node) -> void:
	behaviour.name = "Actor"
	node.add_child(behaviour)


static func colour_of(node: Node2D) -> String:
	# `yellow` is the .ent default and maps to the unsuffixed sprite, so absence means
	# yellow rather than "no colour".
	return String(node.get_meta("color", "yellow"))


# ---------------------------------------------------------------------------


class KeyItem:
	extends Node
	## A carryable key. Free → carried → consumed at a matching keyhole.
	##
	## There is NO physics contact callback for a key anywhere in the binary; everything is
	## distance-polled from a per-frame update, so pickup is a range test rather than an
	## overlap. Polled on a staggered timer (rule 6 forbids per-frame target scanning).

	const TICK_S: float = 0.08

	var carrier: Node2D = null
	var pickup_range: float = 40.0
	var follow_offset: Vector2 = Vector2(0, -34)
	var _next: float = 0.0

	func _ready() -> void:
		pickup_range = float(DataLoader.get_value("actors", ["key", "pickup_range_px"], 40.0))

	func _physics_process(delta: float) -> void:
		var parent: Node2D = get_parent() as Node2D
		if carrier != null:
			if not is_instance_valid(carrier):
				carrier = null
				return
			# Detached from physics while carried — it floats with its holder.
			parent.global_position = carrier.global_position + follow_offset
			return
		_next -= delta
		if _next > 0.0:
			return
		_next = TICK_S
		var player: Node2D = _find_player(parent)
		if player != null and player.global_position.distance_to(parent.global_position) <= pickup_range:
			take(player)

	func _find_player(parent: Node2D) -> Node2D:
		var level: Node = parent.get_parent()
		if level == null:
			return null
		for child: Node in level.get_children():
			if child.is_in_group("player"):
				return child as Node2D
		return null

	func take(new_carrier: Node2D) -> void:
		if carrier != null:
			return
		carrier = new_carrier
		var parent: Node2D = get_parent() as Node2D
		# Stop it colliding while carried, or it shoves the player it is following.
		for child: Node in parent.get_children():
			var body: CollisionObject2D = child as CollisionObject2D
			if body != null:
				body.process_mode = Node.PROCESS_MODE_DISABLED
		EventBus.key_taken.emit(parent, Actors.colour_of(parent))

	func consume() -> void:
		carrier = null
		var parent: Node2D = get_parent() as Node2D
		if is_instance_valid(parent):
			parent.queue_free()


class Keyhole:
	extends Node
	## A lock socket. When a key of its own colour arrives, both the key and every
	## same-colour `door_locked` in the level are removed.
	##
	## Pairing is BY COLOUR, never by proximity: per level the colour multisets of key,
	## keyhole and door_locked are identical, but a keyhole and its door can sit 200–340 px
	## apart, so a nearest-neighbour heuristic would mis-pair them.

	const TICK_S: float = 0.1

	var range_px: float = 40.0
	var _runtime: LevelRuntime
	var _next: float = 0.0

	func _init(runtime: LevelRuntime = null) -> void:
		_runtime = runtime

	func _ready() -> void:
		range_px = float(DataLoader.get_value("actors", ["key", "unlock_range_px"], 40.0))

	func _physics_process(delta: float) -> void:
		_next -= delta
		if _next > 0.0:
			return
		_next = TICK_S
		if _runtime == null:
			return
		var parent: Node2D = get_parent() as Node2D
		var colour: String = Actors.colour_of(parent)
		for key_node: Node2D in _runtime.nodes_with_role(&"key"):
			if not is_instance_valid(key_node) or Actors.colour_of(key_node) != colour:
				continue
			if key_node.global_position.distance_to(parent.global_position) > range_px:
				continue
			_unlock(colour, key_node)
			return

	func _unlock(colour: String, key_node: Node2D) -> void:
		var item: Node = key_node.get_node_or_null("Actor")
		if item != null and item.has_method("consume"):
			item.consume()
		for door: Node2D in _runtime.nodes_with_role(&"locked_door"):
			if is_instance_valid(door) and Actors.colour_of(door) == colour:
				door.queue_free()
		var parent: Node2D = get_parent() as Node2D
		EventBus.lock_opened.emit(colour)
		# The socket itself stays — it is wall furniture — but stops polling.
		if is_instance_valid(parent):
			queue_free()


class Spawner:
	extends Node
	## An invisible `minion_spawn` marker. Creates its enemy once, at level start.

	var _runtime: LevelRuntime

	func _init(runtime: LevelRuntime = null) -> void:
		_runtime = runtime

	func _ready() -> void:
		call_deferred("_spawn")

	func _spawn() -> void:
		var parent: Node2D = get_parent() as Node2D
		if _runtime == null or _runtime.level_root == null or not is_instance_valid(parent):
			return

		var minion: CharacterBody2D = CharacterBody2D.new()
		minion.name = "Minion"
		minion.global_position = parent.global_position
		minion.add_to_group("enemy")
		# A minion is a puzzle object as much as a threat: breakable, burnable and
		# teleportable are all set on the original, so the destruction economy and the
		# portal system must both accept it.
		minion.set_meta("entity_name", "minion")
		minion.set_meta("breakable", "1")
		minion.set_meta("burnable", "1")
		minion.set_meta("teleportable", "1")

		var shape: CollisionShape2D = CollisionShape2D.new()
		var capsule: CapsuleShape2D = CapsuleShape2D.new()
		capsule.radius = 10.0
		capsule.height = 36.0
		shape.shape = capsule
		minion.add_child(shape)

		var brain: Minion = Minion.new()
		brain.name = "Brain"
		brain.configure(_runtime, String(parent.get_meta("waypointName", "")))
		minion.add_child(brain)

		_runtime.level_root.add_child(minion)
		EventBus.enemy_spawned.emit(minion)


class Minion:
	extends Node
	## Patrol between two waypoints, with a line-of-sight alert.
	##
	## `waypointName` is a PREFIX: `wayA` → patrol `wayA0` ↔ `wayA1`. A lone `wayX0` with a
	## sentinel `holdTime` of 9999999 is an intentional stationary guard, so a single
	## resolved waypoint means "stand still", not "data missing".
	##
	## Sight is occluded by `anti_sight_wall.ent`, an invisible bar the original uses so a
	## minion cannot see through specific walls. Modelled with a physics raycast against
	## level geometry, which is the closest honest equivalent.

	const TICK_S: float = 0.12

	var points: Array[Vector2] = []
	var holds_s: Array[float] = []
	var speed: float = 60.0
	var sight_range: float = 220.0

	var _runtime: LevelRuntime
	var _index: int = 0
	var _wait_left: float = 0.0
	var _next_think: float = 0.0
	var _alerted: bool = false

	func configure(runtime: LevelRuntime, prefix: String) -> void:
		_runtime = runtime
		if prefix.is_empty():
			return
		# Markers are named <prefix>0 and <prefix>1.
		for suffix: String in ["0", "1"]:
			var marker: Node2D = runtime.marker_named(prefix + suffix)
			if marker == null:
				continue
			points.append(marker.position)
			# holdTime is MILLISECONDS. The sentinel values (9999999 / 999999999) mean
			# "never leave", so they are clamped rather than taken literally.
			var hold_ms: float = String(marker.get_meta("holdTime", "0")).to_float()
			holds_s.append(minf(hold_ms / 1000.0, 3600.0))

	func _ready() -> void:
		speed = float(DataLoader.get_value("actors", ["minion", "patrol_speed_px_s"], 60.0))
		sight_range = float(DataLoader.get_value("actors", ["minion", "sight_range_px"], 220.0))

	func _physics_process(delta: float) -> void:
		var body: CharacterBody2D = get_parent() as CharacterBody2D
		if body == null:
			return

		# Thinking is staggered; MOVING is every frame (rule 6: interpolate per frame,
		# think on a timer).
		_next_think -= delta
		if _next_think <= 0.0:
			_next_think = TICK_S
			_look(body)

		if points.size() < 2:
			return  # stationary guard
		if _wait_left > 0.0:
			_wait_left -= delta
			return

		var target: Vector2 = points[_index]
		body.global_position = body.global_position.move_toward(target, speed * delta)
		if body.global_position.distance_to(target) < 1.0:
			_wait_left = holds_s[_index] if _index < holds_s.size() else 0.0
			_index = (_index + 1) % points.size()

	func _look(body: CharacterBody2D) -> void:
		var player: Node2D = null
		for child: Node in body.get_parent().get_children():
			if child.is_in_group("player"):
				player = child as Node2D
				break
		if player == null:
			return
		var visible: bool = (
			body.global_position.distance_to(player.global_position) <= sight_range
			and _has_line_of_sight(body, player)
		)
		if visible and not _alerted:
			_alerted = true
			EventBus.enemy_alerted.emit(body, true)
		elif not visible and _alerted:
			_alerted = false
			EventBus.enemy_alerted.emit(body, false)

	func _has_line_of_sight(body: CharacterBody2D, player: Node2D) -> bool:
		var space: PhysicsDirectSpaceState2D = body.get_world_2d().direct_space_state
		var query: PhysicsRayQueryParameters2D = PhysicsRayQueryParameters2D.create(
			body.global_position, player.global_position
		)
		query.exclude = [body.get_rid()]
		var hit: Dictionary = space.intersect_ray(query)
		# Nothing between us, or the first thing hit IS the player.
		return hit.is_empty() or hit.get("collider") == player
