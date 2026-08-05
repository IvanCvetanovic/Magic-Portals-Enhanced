class_name Behaviours
extends Node
## Generic, data-driven entity behaviours.
##
## CLAUDE.md rule 5: one generic treatment plus data, not a class per entity type. The
## original has ~178 entity names but only a handful of distinct *behaviours*, so this is
## a small set of parameterised components that `LevelRuntime` attaches by role.
##
## THE AUTHORITATIVE LIVE-BEHAVIOUR TEST. The engine builds its per-entity callback name as
## `"ETHCallback_" + RemoveExtension(entityName)`, so an entity has behaviour **iff** a
## matching `ETHCallback_*` symbol exists in `android_game.bin`. 96 do. Everything else is
## scenery, however suggestive its custom data looks — `moving_platform_single.ent` carries
## a `moveStride` that nothing reads, and inline `platform` and `anchor` carry speed/stride
## that are dead copy-paste. Implementing those would invent motion the original never had.
##
## THE `stride` DISAMBIGUATION RULE, applied throughout:
##   * paired with a `speed` float  → `stride` is a DISTANCE IN PIXELS
##   * a bare uint with no `speed`  → `stride` is a DURATION IN MILLISECONDS
## Corroborated by `carranca`, which carries both `stride` and `startStride` (an
## unambiguous initial delay), and by `button`'s 300 on a 26×16 body — not a distance.

## Oscillation-rate scale. `speed` is a frequency, not a velocity: across the 15
## `moving_platform` placements `speed × stride` clusters in a narrow band while each
## factor alone varies widely, which is the signature of frequency × amplitude.
## ⚠ GUESS — the absolute magnitude needs the bytecode constants.
const RATE_SCALE_K: float = 2.0


## Attach the behaviour for `role` to `node`, if that role has one.
static func attach(node: Node2D, role: StringName, runtime: LevelRuntime) -> void:
	match role:
		&"moving_platform":
			_attach(node, Mover.oscillating(node))
		&"lift":
			_attach(node, Mover.shuttle(node, runtime))
		&"switched_door":
			_attach(node, Mover.gated(node))
		&"switch":
			_attach(node, Switch.new())
		&"collectible":
			# Only the inline `crystal` carries `time`; `crystal.ent` never does.
			if node.has_meta("time"):
				_attach(node, TimedCollectible.new())
		&"reflector":
			_attach(node, Reflector.new())
		&"hinge":
			_attach(node, Hinge.new())
		_:
			pass


static func _attach(node: Node2D, behaviour: Node) -> void:
	behaviour.name = "Behaviour"
	node.add_child(behaviour)


static func meta_float(node: Node2D, key: String, fallback: float) -> float:
	return float(String(node.get_meta(key))) if node.has_meta(key) else fallback


static func meta_int(node: Node2D, key: String, fallback: int) -> int:
	return int(String(node.get_meta(key))) if node.has_meta(key) else fallback


# ---------------------------------------------------------------------------


class Mover:
	extends Node
	## Moves its parent. Three profiles, one update loop.
	##
	## The parent is re-parented onto an `AnimatableBody2D` by `LevelRuntime` when it has
	## collision, because a teleported `StaticBody2D` does **not** carry riders — the
	## player would slide off a platform instead of moving with it.

	enum Profile { OSCILLATE, SHUTTLE, GATED }

	var profile: Profile = Profile.OSCILLATE
	var origin: Vector2 = Vector2.ZERO
	var axis: Vector2 = Vector2.DOWN
	var travel_px: float = 0.0
	var rate: float = 1.0
	var pose_a: Vector2 = Vector2.ZERO
	var pose_b: Vector2 = Vector2.ZERO
	var speed_px_s: float = 120.0
	var duration_s: float = 1.0

	var _t: float = 0.0
	var _progress: float = 0.0
	var _forward: bool = true
	var _open: bool = false

	static func oscillating(node: Node2D) -> Mover:
		var mover: Mover = Mover.new()
		mover.profile = Profile.OSCILLATE
		mover.origin = node.position
		# VERTICAL, always. There is no per-instance direction key, and the geometry rules
		# horizontal out: level 3-21's platform is the exact 64 px gap between two slabs
		# that share its y-centre, with a stride of 110 — it cannot move sideways.
		mover.axis = Vector2.DOWN
		mover.travel_px = Behaviours.meta_float(node, "stride", 64.0)
		mover.rate = Behaviours.meta_float(node, "speed", 1.0)
		return mover

	static func shuttle(node: Node2D, runtime: LevelRuntime) -> Mover:
		var mover: Mover = Mover.new()
		mover.profile = Profile.SHUTTLE
		# `a`/`b` are ENTITY NAMES of invisible marker entities, not coordinates or
		# indices. In all 14 placements the lift's own position is byte-identical to
		# marker `a`, so it spawns on `a` and shuttles to `b`.
		mover.pose_a = runtime.marker_position(String(node.get_meta("a", "")), node.position)
		mover.pose_b = runtime.marker_position(String(node.get_meta("b", "")), node.position)
		mover.speed_px_s = Behaviours.meta_float(node, "speed", 120.0)
		return mover

	static func gated(node: Node2D) -> Mover:
		var mover: Mover = Mover.new()
		mover.profile = Profile.GATED
		mover.pose_a = node.position
		# A door_lift's collision box is 30×126, and it opens by sinking its own height.
		mover.pose_b = node.position + Vector2(0, -126)
		# Bare uint with no `speed` ⇒ milliseconds.
		mover.duration_s = maxf(Behaviours.meta_float(node, "stride", 1000.0) / 1000.0, 0.01)
		return mover

	func set_open(open: bool) -> void:
		_open = open

	func _physics_process(delta: float) -> void:
		var parent: Node2D = get_parent() as Node2D
		if parent == null:
			return
		match profile:
			Profile.OSCILLATE:
				_t += delta
				# Centred on the AUTHORED position, so the visual error is bounded by
				# travel/2 whichever phase convention the original used — which is
				# genuinely unknown (three models fit the geometry equally well).
				var offset: float = sin(RATE_SCALE_K * rate * _t) * travel_px * 0.5
				parent.position = origin + axis * offset
			Profile.SHUTTLE:
				var target: Vector2 = pose_b if _forward else pose_a
				parent.position = parent.position.move_toward(target, speed_px_s * delta)
				if parent.position.is_equal_approx(target):
					_forward = not _forward
			Profile.GATED:
				# Reversal mid-travel resumes from the current position rather than
				# snapping — chapter 2's "close all doors before finishing" achievement
				# means a door must be able to travel back.
				var step: float = delta / duration_s
				_progress = clampf(_progress + (step if _open else -step), 0.0, 1.0)
				parent.position = pose_a.lerp(pose_b, _progress)


class Switch:
	extends Node
	## A pressure plate. Publishes on a numbered channel while anything rests on it.
	##
	## NOT a latch: the original tracks a touching-entity count and has both begin- and
	## end-contact callbacks, so the plate releases the instant the last body leaves.

	var channel: int = -1
	var _count: int = 0

	func _ready() -> void:
		var parent: Node2D = get_parent() as Node2D
		channel = Behaviours.meta_int(parent, "idx", -1)
		var trigger: Area2D = parent.get_node_or_null("Trigger")
		if trigger == null:
			return
		trigger.body_entered.connect(_on_entered)
		trigger.body_exited.connect(_on_exited)

	func _on_entered(_body: Node2D) -> void:
		_count += 1
		if _count == 1:
			EventBus.channel_changed.emit(channel, true)

	func _on_exited(_body: Node2D) -> void:
		_count = maxi(0, _count - 1)
		if _count == 0:
			EventBus.channel_changed.emit(channel, false)


class TimedCollectible:
	extends Node
	## A crystal that expires. Only the inline `crystal` carries `time` (5000–12000 ms);
	## `crystal.ent` never does and is collected at leisure.

	var _left: float = 0.0

	func _ready() -> void:
		_left = Behaviours.meta_float(get_parent() as Node2D, "time", 0.0) / 1000.0

	func _process(delta: float) -> void:
		if _left <= 0.0:
			return
		_left -= delta
		var parent: Node2D = get_parent() as Node2D
		if _left <= 0.0:
			parent.queue_free()
			return
		# Blink faster as it runs out. GUESS — `crystal_temp_alert.mp3` exists, so the
		# original does warn the player somehow, but not necessarily like this.
		if _left < 2.0:
			parent.modulate.a = 0.4 + 0.6 * absf(sin(_left * 12.0))


class Reflector:
	extends Node
	## Mirrors things that cross its plane. `plane` is `vertical` (6) or `horizontal` (3).
	##
	## ⚠ GUESS as to WHAT it reflects. The binary has `ETHCallback_reflect_agent`,
	## `portal_reflect.mp3` and `projectile_reflect.mp3`, so both a portal and a projectile
	## can be reflected. Implemented as mirroring a traveller's velocity across the plane,
	## which is the reading that makes the sound names sensible.

	var normal: Vector2 = Vector2.RIGHT

	func _ready() -> void:
		var parent: Node2D = get_parent() as Node2D
		var plane: String = String(parent.get_meta("plane", "vertical"))
		normal = Vector2.RIGHT if plane == "vertical" else Vector2.DOWN
		var trigger: Area2D = parent.get_node_or_null("Trigger")
		if trigger != null:
			trigger.body_entered.connect(_on_entered)

	func _on_entered(body: Node2D) -> void:
		if body is CharacterBody2D:
			var character: CharacterBody2D = body
			character.velocity = character.velocity.bounce(normal)
		elif body is RigidBody2D:
			var rigid: RigidBody2D = body
			rigid.linear_velocity = rigid.linear_velocity.bounce(normal)
		EventBus.reflected.emit(body)


class Hinge:
	extends Node
	## A revolute joint — seesaws, spinning platforms, motor seesaws.
	##
	## The joint data is nested inside `<Collision>` in the level file, alongside
	## `<Polygon>` and `<Compound>` (the same nesting that hid polygons earlier). The
	## converter resolves the anchor into world units and emits it as metadata.
	##
	## **Angles are RADIANS.** The shipped `±1.57075` is `±π/2`, which is what settles the
	## units question the format notes left open — a degree reading would be nonsensical.
	##
	## Only 10 instances exist in the whole game, all in worlds 2-4.

	func _ready() -> void:
		var parent: Node2D = get_parent() as Node2D
		var body: Node = parent.get_node_or_null("Body")
		var rigid: RigidBody2D = body as RigidBody2D
		if rigid == null:
			# A hinge on a static body cannot rotate. Report rather than silently do
			# nothing — it would mean the converter emitted the wrong body type.
			push_warning(
				"Hinge: %s has no RigidBody2D to pivot" % parent.get_meta("entity_name", "?")
			)
			return

		var pin: PinJoint2D = PinJoint2D.new()
		pin.name = "Pin"
		# The anchor is normalised -1..1 of the collision half-box in the source data; the
		# converter has already scaled it, so this is a plain local offset.
		pin.position = Vector2(
			Behaviours.meta_float(parent, "joint_anchor_x", 0.0),
			Behaviours.meta_float(parent, "joint_anchor_y", 0.0)
		)
		pin.node_a = rigid.get_path()

		if Behaviours.meta_float(parent, "joint_enable_limit", 0.0) != 0.0:
			pin.angular_limit_enabled = true
			pin.angular_limit_lower = Behaviours.meta_float(parent, "joint_lower_angle", 0.0)
			pin.angular_limit_upper = Behaviours.meta_float(parent, "joint_upper_angle", TAU)
		if Behaviours.meta_float(parent, "joint_enable_motor", 0.0) != 0.0:
			pin.motor_enabled = true
			pin.motor_target_velocity = Behaviours.meta_float(parent, "joint_motor_speed", 0.0)

		parent.add_child(pin)
