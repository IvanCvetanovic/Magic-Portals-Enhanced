class_name PortalSystem
extends Node
## Portal placement and traversal — MILESTONE_1.md §3.
##
## THE LOOP (OWNER testimony, 2026-08-05, see `docs/original-gameplay.md` §2.0):
##   * movement is two buttons; **portals are placed by tapping the world**
##   * **at most two player portals exist at once** — one pair
##   * **passing through destroys the pair** — they are one-shot, not permanent
##   * and that applies to any `teleportable` object, not only the player
##
## So a level is a puzzle about *where* to spend a limited number of single-use pairs, and
## the golden score is a budget on how many pairs you spend getting out.
##
## STATIC portals (`portal_static`) are treated as PERMANENT. That is reasoned, not
## confirmed: the name says static, the engine has a dedicated
## `teleportToFirstStaticPortal`, and level 1-1 would be unsolvable after two uses if its
## four static portals were consumed. Flagged in the doc as still open.
##
## The index space is SHARED between static and placed portals — level 1-2 ships a static
## portal whose `destiny` names a portal the player has not created yet.

## Hard cap on simultaneous player portals (OWNER). A level may allow FEWER via its
## `properties` entity's `max_portals`, never more.
const PAIR_SIZE: int = 2

## Portals created this attempt, all-time. This is what the golden score measures — a
## running total, not a simultaneous count, so it keeps rising as pairs are spent.
var portals_used: int = 0

var _runtime: LevelRuntime = null
var _player: Player = null
var _static_by_index: Dictionary = {}
var _placed: Array[Node2D] = []
var _placed_budget: int = 0
var _lockout_left: float = 0.0

var _exit_offset: float = 12.0
var _reentry_lockout: float = 0.2
var _momentum_mode: String = "rotate_to_exit"
var _exit_speed_scale: float = 1.0
var _radius: float = 16.0
var _consume_on_traverse: bool = true
var _statics_persist: bool = true
var _recycle_oldest: bool = true


func _ready() -> void:
	_exit_offset = float(DataLoader.get_value("portals", ["transit", "exit_offset_px"], 12.0))
	_reentry_lockout = float(DataLoader.get_value("portals", ["transit", "reentry_lockout_s"], 0.2))
	_momentum_mode = String(
		DataLoader.get_value("portals", ["transit", "momentum_mode"], "rotate_to_exit")
	)
	_exit_speed_scale = float(DataLoader.get_value("portals", ["transit", "exit_speed_scale"], 1.0))
	_radius = float(DataLoader.get_value("portals", ["placement", "collision_radius_px"], 16.0))
	# OWNER-confirmed behaviour, still expressed as data so playtesting can flip it without
	# a code change (CLAUDE.md rule 1).
	_consume_on_traverse = bool(
		DataLoader.get_value("portals", ["consumption", "consume_on_traverse"], true)
	)
	_statics_persist = bool(
		DataLoader.get_value("portals", ["consumption", "static_portals_persist"], true)
	)
	_recycle_oldest = bool(
		DataLoader.get_value("portals", ["consumption", "on_cap_recycle_oldest"], true)
	)
	EventBus.intent_pressed.connect(_on_intent_pressed)


func bind(runtime: LevelRuntime) -> void:
	_runtime = runtime
	_static_by_index.clear()
	_placed.clear()
	portals_used = 0
	_lockout_left = 0.0
	# A level with no `properties` entity gets the default; one that explicitly stores 0
	# (level 1-1) grants no placement at all and must be solved with its static portals.
	_placed_budget = mini(runtime.max_portals, PAIR_SIZE)

	for node: Node2D in runtime.nodes_with_role(&"static_portal"):
		if not node.has_meta("index"):
			continue
		if node.has_meta("active") and int(String(node.get_meta("active"))) == 0:
			continue
		_static_by_index[int(String(node.get_meta("index")))] = node
		var trigger: Area2D = node.get_node_or_null("Trigger")
		if trigger != null:
			trigger.body_entered.connect(_on_portal_entered.bind(node))


func set_player(player: Player) -> void:
	_player = player


func _physics_process(delta: float) -> void:
	_lockout_left = maxf(0.0, _lockout_left - delta)


# --- placement ------------------------------------------------------------


func _on_intent_pressed(intent: int, screen_position: Vector2) -> void:
	if intent != InputController.Intent.PRIMARY or _runtime == null:
		return
	# InputController reports viewport coordinates — it has no business knowing about
	# cameras. Converting here keeps that boundary intact.
	var canvas: Transform2D = get_viewport().get_canvas_transform()
	try_place(canvas.affine_inverse() * screen_position)


## Attempt to place a portal. Returns false (and why, via push_warning) when refused.
func try_place(world_position: Vector2) -> bool:
	if _placed_budget <= 0:
		return false
	if _is_blocked(world_position):
		# `playPortalFailedSound` exists in the original, so refusal is a real, audible
		# outcome rather than a silent no-op.
		return false

	# At the cap, the OLDEST portal gives way. GUESS: the original might instead refuse,
	# or clear both. Recycling keeps the player unstuck, which is the safer failure mode
	# for a puzzle game — but it is a guess and marked as one in portals.json.
	if _placed.size() >= _placed_budget:
		if not _recycle_oldest:
			return false
		_destroy(_placed[0])

	var portal: Node2D = _make_portal(world_position)
	# Into the LEVEL, so a retry frees placed portals with it.
	_runtime.level_root.add_child(portal)
	_placed.append(portal)
	portals_used += 1
	EventBus.portal_placed.emit(world_position, _placed.size())
	return true


func _is_blocked(world_position: Vector2) -> bool:
	# `isPointInAntiPortalField` in the original. Each antiportal carries a `scale`
	# multiplier in custom data (chapter-1 values 1.8 … 9); the base radius it multiplies
	# is a GUESS.
	for node: Node2D in _runtime.nodes_with_role(&"no_portal_zone"):
		var scale_factor: float = 1.0
		if node.has_meta("scale"):
			scale_factor = float(String(node.get_meta("scale")))
		if world_position.distance_to(node.global_position) <= _radius * scale_factor:
			return true
	return false


func _make_portal(world_position: Vector2) -> Node2D:
	var portal: Node2D = Node2D.new()
	portal.name = "PlacedPortal%d" % portals_used
	portal.global_position = world_position
	portal.set_meta("placed", true)

	var area: Area2D = Area2D.new()
	area.name = "Trigger"
	area.monitorable = false
	var shape: CollisionShape2D = CollisionShape2D.new()
	var circle: CircleShape2D = CircleShape2D.new()
	circle.radius = _radius
	shape.shape = circle
	area.add_child(shape)
	portal.add_child(area)
	area.body_entered.connect(_on_portal_entered.bind(portal))
	return portal


func _destroy(portal: Node2D) -> void:
	_placed.erase(portal)
	if is_instance_valid(portal):
		portal.queue_free()


## Clear all placed portals — the original has addClearPortalsButton/removeClearPortalsButton.
func clear_placed() -> void:
	for portal: Node2D in _placed.duplicate():
		_destroy(portal)


# --- traversal ------------------------------------------------------------


func _on_portal_entered(body: Node2D, portal: Node2D) -> void:
	if _lockout_left > 0.0 or not is_instance_valid(portal):
		return
	if not _can_travel(body):
		return
	var exit_portal: Node2D = _partner_of(portal)
	if exit_portal == null or exit_portal == portal:
		return
	_teleport(body, portal, exit_portal)


func _can_travel(body: Node2D) -> bool:
	if body == _player:
		return true
	# OWNER: other items travel too. The original gates that per-instance with a
	# `teleportable` flag in custom data — crate.ent=1 but bare crate=0 — so the same
	# logical object can be portable in one placement and not in another.
	var owner_node: Node = body.get_parent()
	if owner_node != null and owner_node.has_meta("teleportable"):
		return int(String(owner_node.get_meta("teleportable"))) != 0
	return false


## The portal a traveller entering `portal` should come out of.
func _partner_of(portal: Node2D) -> Node2D:
	if portal.has_meta("placed"):
		# Placed portals pair with each other, oldest-to-newest. With PAIR_SIZE 2 this is
		# simply "the other one".
		for other: Node2D in _placed:
			if other != portal and is_instance_valid(other):
				return other
		return null

	# A static portal names its partner by index. That index may belong to a portal the
	# player has yet to place (level 1-2), in which case fall through to the placed set.
	if not portal.has_meta("destiny"):
		return null
	var destiny: int = int(String(portal.get_meta("destiny")))
	var target: Variant = _static_by_index.get(destiny)
	if target != null:
		return target
	for other: Node2D in _placed:
		if is_instance_valid(other):
			return other
	return null


func _teleport(traveller: Node2D, entry: Node2D, exit_portal: Node2D) -> void:
	var velocity: Vector2 = Vector2.ZERO
	if traveller is CharacterBody2D:
		velocity = (traveller as CharacterBody2D).velocity
	elif traveller is RigidBody2D:
		velocity = (traveller as RigidBody2D).linear_velocity

	var exit_velocity: Vector2 = velocity
	match _momentum_mode:
		"reset":
			exit_velocity = Vector2.ZERO
		"rotate_to_exit":
			exit_velocity = velocity.rotated(exit_portal.rotation - entry.rotation)
		_:
			exit_velocity = velocity
	exit_velocity *= _exit_speed_scale

	# Clear of the exit along the travel direction, or the traveller lands inside the exit
	# trigger and bounces back the moment the lockout expires.
	var direction: Vector2 = (
		exit_velocity.normalized() if not exit_velocity.is_zero_approx() else Vector2.DOWN
	)
	var destination: Vector2 = exit_portal.global_position + direction * _exit_offset

	if traveller is Player:
		(traveller as Player).teleport_to(destination, exit_velocity)
	elif traveller is RigidBody2D:
		var rigid: RigidBody2D = traveller
		rigid.global_position = destination
		rigid.linear_velocity = exit_velocity
	else:
		traveller.global_position = destination

	_lockout_left = _reentry_lockout
	EventBus.portal_traversed.emit(
		traveller,
		int(String(entry.get_meta("index", "-1"))),
		int(String(exit_portal.get_meta("index", "-1"))),
	)

	# OWNER: passing through destroys the pair. Only PLACED portals are consumed — static
	# ones are assumed permanent (see the class docstring). Both ends go, because they are
	# one linked pair, not two independent portals.
	if not _consume_on_traverse:
		return
	var consumed: bool = false
	for portal: Node2D in [entry, exit_portal]:
		# Statics are assumed permanent unless the data says otherwise.
		if portal.has_meta("placed") or not _statics_persist:
			_destroy(portal)
			consumed = true
	if consumed:
		EventBus.portals_consumed.emit(_placed.size())


# --- test hooks -----------------------------------------------------------
# Kept minimal and clearly labelled. verify_gameplay drives traversal directly because
# waiting for physics to push a body through a trigger is slow and flaky, and what is
# under test is the consumption rule, not Godot's collision detection.


## Number of placed portals currently alive.
func get_placed_count() -> int:
	var live: int = 0
	for portal: Node2D in _placed:
		if is_instance_valid(portal) and not portal.is_queued_for_deletion():
			live += 1
	return live


## Force `traveller` through the current placed pair, bypassing trigger detection.
func force_traverse_for_test(traveller: Node2D) -> bool:
	if _placed.size() < 2:
		return false
	_lockout_left = 0.0
	_teleport(traveller, _placed[0], _placed[1])
	return true
