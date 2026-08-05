class_name Hazards
extends Node
## The destruction economy: what breaks what, and what kills the player.
##
## The original reads four per-instance flags through a single predicate it literally
## names `isBreakableOrExplosiveOrBurnable`:
##
##   `demolisher` (rolling_stone, 27/27) destroys anything flagged `breakable`
##   `explosive`  (barrel_bomb, 31/31)   detonates, and chains to other explosives
##   `burnable`   (crates, shock_agents) enters a TIMED burning state, then is removed
##   `breakable`  (walls, carrancas)     marks what can be smashed
##
## So every hazard is either a source of destruction or a target of it, and almost all of
## it is `teleportable` — which is the whole puzzle: route a destructive object onto a
## destructible one *through a portal pair*.
##
## Burning removes the body **entirely, collision included**. That removal IS the puzzle —
## burning a crate deletes a platform or a blocker. It is also reversible upstream
## (`healBurn`, `extinguish_fire.ent`, `playFireExtinguishSound`), which is not modelled
## because nothing in the level data puts out a fire.
##
## Every duration, radius and force here comes from `res://data/hazards.json` and is a
## marked GUESS — the original's constants are inside AngelScript bytecode we can name but
## not yet read (`docs/original-gameplay.md` §5).


static func attach(node: Node2D, role: StringName, runtime: LevelRuntime) -> void:
	match role:
		&"demolisher":
			_add(node, Demolisher.new())
		&"explosive":
			_add(node, Explosive.new())
		&"fire_source":
			_add(node, FireSource.new())
		&"shock_field":
			_add(node, ShockField.new())
		&"gravity_well":
			_add(node, GravityWell.new())
		&"turret":
			_add(node, Turret.new(runtime))
		&"launcher":
			_add(node, Launcher.new(runtime))
		_:
			pass
	# Flags are orthogonal to role: a crate is `burnable` AND `teleportable`, a wall is
	# `breakable` and may also `blink`. Applied to every entity regardless of its role.
	if _flag(node, "burnable"):
		_add(node, Burnable.new(), "Burnable")
	if _flag(node, "blink"):
		_add(node, Blink.new(), "Blink")


static func _add(node: Node2D, behaviour: Node, name: String = "Hazard") -> void:
	behaviour.name = name
	node.add_child(behaviour)


static func _flag(node: Node2D, key: String) -> bool:
	if not node.has_meta(key):
		return false
	var raw: String = String(node.get_meta(key))
	return raw != "" and raw != "0"


static func _num(node: Node2D, key: String, fallback: float) -> float:
	return String(node.get_meta(key)).to_float() if node.has_meta(key) else fallback


## Destroy an entity and announce it, once. The node may already be queued by a chain
## reaction — two explosives can each request the other in the same frame.
static func destroy(node: Node2D, cause: StringName) -> void:
	if not is_instance_valid(node) or node.is_queued_for_deletion():
		return
	EventBus.entity_destroyed.emit(node, cause)
	node.queue_free()


## The entity a physics body belongs to. The converter nests the body under the entity
## node, so a contact reports the body and callers need its owner.
static func entity_of(body: Node) -> Node2D:
	var current: Node = body
	while current != null:
		var node: Node2D = current as Node2D
		if node != null and node.has_meta("entity_name"):
			return node
		current = current.get_parent()
	return null


# ---------------------------------------------------------------------------


class Demolisher:
	extends Node
	## A rolling stone. Smashes anything `breakable` it touches.
	##
	## NOT a touch-kill hazard: in level 1-9 — the level the "Like a rolling stone"
	## achievement names — the player spawns 58 px from one on the same platform, so it
	## must be safe to stand beside. It is a physics prop that happens to break things.

	func _ready() -> void:
		var parent: Node2D = get_parent() as Node2D
		var body: Node = parent.get_node_or_null("Body")
		if body == null:
			body = parent.get_node_or_null("MovingBody")
		var rigid: RigidBody2D = body as RigidBody2D
		if rigid == null:
			return
		rigid.contact_monitor = true
		rigid.max_contacts_reported = 4
		rigid.body_entered.connect(_on_hit)

	func _on_hit(other: Node) -> void:
		var target: Node2D = Hazards.entity_of(other)
		if target != null and Hazards._flag(target, "breakable"):
			Hazards.destroy(target, &"demolished")


class Explosive:
	extends Node
	## A barrel bomb. Inert on contact — the binary has no
	## `ETHBeginContactCallback_barrel_bomb`, only a per-frame callback whose single local
	## is `brightness`, i.e. an emissive "live bomb" pulse. Detonation is REQUESTED by
	## something else (`hasExplodeRequest`), so this exposes `detonate()` rather than
	## triggering itself.

	var radius: float = 96.0

	func _ready() -> void:
		radius = float(DataLoader.get_value("hazards", ["explosion", "radius_px"], 96.0))

	func detonate() -> void:
		var parent: Node2D = get_parent() as Node2D
		if not is_instance_valid(parent) or parent.is_queued_for_deletion():
			return
		var origin: Vector2 = parent.global_position
		var level: Node = parent.get_parent()
		# Free self FIRST, so a chain cannot re-enter this bomb and recurse.
		Hazards.destroy(parent, &"exploded")
		if level == null:
			return
		for child: Node in level.get_children():
			var other: Node2D = child as Node2D
			if other == null or not other.has_meta("entity_name"):
				continue
			if other.global_position.distance_to(origin) > radius:
				continue
			if Hazards._flag(other, "breakable"):
				Hazards.destroy(other, &"exploded")
			elif Hazards._flag(other, "explosive"):
				# Chain reaction, deferred so it lands in the next frame rather than
				# recursing through an arbitrarily long chain in one call.
				var chained: Node = other.get_node_or_null("Hazard")
				if chained != null and chained.has_method("detonate"):
					chained.call_deferred("detonate")
			elif Hazards._flag(other, "burnable"):
				var burnable: Node = other.get_node_or_null("Burnable")
				if burnable != null and burnable.has_method("ignite"):
					burnable.ignite()


class Burnable:
	extends Node
	## A timed burning state, ending in complete removal — collision included.
	##
	## Applies to two very different things: wooden crates and shock_agents. Five
	## achievements across chapter 2 are gated on burning, or *not* burning, a specific
	## crate, so the state has to be a real duration the player can race.

	var burn_time: float = 2.0
	var _left: float = -1.0

	func _ready() -> void:
		burn_time = float(DataLoader.get_value("hazards", ["burning", "burn_time_s"], 2.0))

	func is_burning() -> bool:
		return _left > 0.0

	func ignite() -> void:
		if _left > 0.0:
			return
		_left = burn_time
		EventBus.entity_ignited.emit(get_parent() as Node2D)

	func _process(delta: float) -> void:
		if _left <= 0.0:
			return
		_left -= delta
		var parent: Node2D = get_parent() as Node2D
		# Tint toward fire while it burns, so the timer is legible without a particle
		# system. The original attaches a fire32.ent plume; this is the honest stand-in.
		parent.modulate = Color(1.0, 0.55, 0.25).lerp(Color.WHITE, _left / maxf(burn_time, 0.01))
		if _left <= 0.0:
			Hazards.destroy(parent, &"burned")


class FireSource:
	extends Node
	## A permanent open flame — the ignition source of the game. No physics body at all
	## (`shape=BS_NONE`); it works on whatever overlaps its box.
	##
	## Ticked on a staggered timer, not every frame: CLAUDE.md rule 6 forbids scanning for
	## targets in `_process`, and an ignition source does not need frame precision.

	const TICK_S: float = 0.15

	var radius: float = 24.0
	var _next: float = 0.0

	func _ready() -> void:
		radius = float(DataLoader.get_value("hazards", ["fire", "reach_px"], 24.0))
		# Stagger the phase so many sources in one level do not all tick on the same frame.
		_next = float(get_instance_id() % 10) * 0.01

	func _process(delta: float) -> void:
		_next -= delta
		if _next > 0.0:
			return
		_next = TICK_S
		var parent: Node2D = get_parent() as Node2D
		var level: Node = parent.get_parent()
		if level == null:
			return
		for child: Node in level.get_children():
			var other: Node2D = child as Node2D
			if other == null or other == parent or not other.has_meta("entity_name"):
				continue
			if other.global_position.distance_to(parent.global_position) > radius:
				continue
			var burnable: Node = other.get_node_or_null("Burnable")
			if burnable != null and burnable.has_method("ignite"):
				burnable.ignite()
			elif Hazards._flag(other, "explosive"):
				var bomb: Node = other.get_node_or_null("Hazard")
				if bomb != null and bomb.has_method("detonate"):
					bomb.call_deferred("detonate")


class ShockField:
	extends Node
	## A blue orb projecting a lethal ring of radius `areaRadius`.
	##
	## The ring is DRAWN, not invisible: the original creates a `shock_area.ent` child —
	## a `white_ring.png` sprite, additively blended, scaled so its radius equals
	## `areaRadius` (the sprite is 128 px, so scale = areaRadius / 64, and the default
	## areaRadius of 64 gives scale 1.0 exactly). The ring is an honest read-out of the
	## danger zone, so hiding it would make the hazard unfair.

	var radius: float = 64.0

	func _ready() -> void:
		var parent: Node2D = get_parent() as Node2D
		radius = Hazards._num(parent, "areaRadius", 64.0)
		var area: Area2D = Area2D.new()
		area.name = "ShockArea"
		area.monitorable = false
		var shape: CollisionShape2D = CollisionShape2D.new()
		var circle: CircleShape2D = CircleShape2D.new()
		circle.radius = radius
		shape.shape = circle
		area.add_child(shape)
		parent.add_child(area)
		area.body_entered.connect(_on_entered)

	func _on_entered(body: Node2D) -> void:
		if body.is_in_group("player") and body.has_method("kill"):
			body.kill(&"shock")


class GravityWell:
	extends Node
	## A local gravity well of radius `radius`, pulling dynamic bodies toward itself.
	##
	## Chapter 4 only — the zero-gravity chapter, where a well is the only thing that
	## moves loose objects at all.

	const TICK_S: float = 0.05

	var radius: float = 128.0
	var strength: float = 400.0
	var _next: float = 0.0

	func _ready() -> void:
		radius = Hazards._num(get_parent() as Node2D, "radius", 128.0)
		strength = float(DataLoader.get_value("hazards", ["gravity_well", "strength_px_s2"], 400.0))

	func _physics_process(delta: float) -> void:
		_next -= delta
		if _next > 0.0:
			return
		_next = TICK_S
		var parent: Node2D = get_parent() as Node2D
		var level: Node = parent.get_parent()
		if level == null:
			return
		for child: Node in level.get_children():
			for body: Node in _bodies_of(child):
				var rigid: RigidBody2D = body as RigidBody2D
				if rigid == null:
					continue
				var offset: Vector2 = parent.global_position - rigid.global_position
				var distance: float = offset.length()
				if distance > radius or distance < 1.0:
					continue
				# Linear falloff rather than inverse-square: it is stable at the centre and
				# reads more predictably in a puzzle. GUESS either way.
				rigid.apply_central_force(offset.normalized() * strength * (1.0 - distance / radius))

	func _bodies_of(node: Node) -> Array[Node]:
		var out: Array[Node] = []
		for child: Node in node.get_children():
			if child is PhysicsBody2D:
				out.append(child)
		return out


class Turret:
	extends Node
	## A carranca — a carved stone gargoyle bolted to a wall, spitting fireballs.
	##
	## Fires the entity named in `entity` (always `fireball.ent`) along `(dirX, dirY)`:
	## first shot after `startStride` ms, then every `stride` ms. Both are bare uints with
	## no `speed`, so both are MILLISECONDS.

	var direction: Vector2 = Vector2.LEFT
	var interval_s: float = 2.0
	var speed: float = 180.0
	var _left: float = 0.0
	var _runtime: LevelRuntime

	func _init(runtime: LevelRuntime = null) -> void:
		_runtime = runtime

	func _ready() -> void:
		var parent: Node2D = get_parent() as Node2D
		direction = Vector2(
			Hazards._num(parent, "dirX", -1.0), Hazards._num(parent, "dirY", 0.0)
		)
		if direction == Vector2.ZERO:
			direction = Vector2.LEFT
		direction = direction.normalized()
		interval_s = maxf(Hazards._num(parent, "stride", 2000.0) / 1000.0, 0.1)
		# startStride is an unambiguous initial delay — its existence alongside `stride` is
		# what proves the millisecond reading for this whole family.
		_left = Hazards._num(parent, "startStride", 0.0) / 1000.0
		speed = float(DataLoader.get_value("hazards", ["turret", "projectile_speed_px_s"], 180.0))

	func _process(delta: float) -> void:
		_left -= delta
		if _left > 0.0:
			return
		_left = interval_s
		_fire()

	func _fire() -> void:
		var parent: Node2D = get_parent() as Node2D
		if _runtime == null or _runtime.level_root == null:
			return
		var shot: Area2D = Area2D.new()
		shot.name = "Fireball"
		shot.global_position = parent.global_position + direction * 24.0
		shot.set_meta("fire", true)
		var shape: CollisionShape2D = CollisionShape2D.new()
		var circle: CircleShape2D = CircleShape2D.new()
		circle.radius = 8.0
		shape.shape = circle
		shot.add_child(shape)
		var mover: Projectile = Projectile.new()
		mover.velocity = direction * speed
		shot.add_child(mover)
		_runtime.level_root.add_child(shot)
		EventBus.projectile_fired.emit(shot)


class Projectile:
	extends Node
	## A fireball. Ignites what it touches, kills the player, and expires on a timer so a
	## turret firing into open space cannot accumulate bodies forever.

	var velocity: Vector2 = Vector2.ZERO
	var life_s: float = 4.0

	func _ready() -> void:
		life_s = float(DataLoader.get_value("hazards", ["turret", "projectile_life_s"], 4.0))
		var area: Area2D = get_parent() as Area2D
		if area != null:
			area.body_entered.connect(_on_hit)

	func _physics_process(delta: float) -> void:
		var area: Area2D = get_parent() as Area2D
		if area == null:
			return
		area.global_position += velocity * delta
		life_s -= delta
		if life_s <= 0.0:
			area.queue_free()

	func _on_hit(body: Node2D) -> void:
		var area: Area2D = get_parent() as Area2D
		if body.is_in_group("player") and body.has_method("kill"):
			body.kill(&"fireball")
		else:
			var target: Node2D = Hazards.entity_of(body)
			if target != null:
				var burnable: Node = target.get_node_or_null("Burnable")
				if burnable != null and burnable.has_method("ignite"):
					burnable.ignite()
		if area != null:
			area.queue_free()


class Launcher:
	extends Node
	## `entity_launcher` — spawns the entity named in `entity` every `stride` ms.
	##
	## The min/max keys are a CULL BOX, not a spawn area: `destroyier.ent` is an invisible
	## sensor that despawns launched objects which fall out of play, and the launcher's own
	## bounds serve the same purpose. Chapter 1 uses this in 1-24, 1-25 and 1-26.

	var interval_s: float = 3.0
	var _left: float = 0.0
	var _runtime: LevelRuntime
	var _bounds: Rect2

	func _init(runtime: LevelRuntime = null) -> void:
		_runtime = runtime

	func _ready() -> void:
		var parent: Node2D = get_parent() as Node2D
		interval_s = maxf(Hazards._num(parent, "stride", 3000.0) / 1000.0, 0.25)
		_left = interval_s
		var min_x: float = Hazards._num(parent, "minX", -1e9)
		var min_y: float = Hazards._num(parent, "minY", -1e9)
		var max_x: float = Hazards._num(parent, "maxX", 1e9)
		var max_y: float = Hazards._num(parent, "maxY", 1e9)
		_bounds = Rect2(Vector2(min_x, min_y), Vector2(max_x - min_x, max_y - min_y))

	func _process(delta: float) -> void:
		_left -= delta
		if _left > 0.0:
			return
		_left = interval_s
		# The launched entity is a converted prop we cannot instance by name at runtime —
		# the converter emits levels, not per-entity scenes. Announce the intent so a
		# future prop library can serve it, rather than silently doing nothing.
		EventBus.spawn_requested.emit(
			String((get_parent() as Node2D).get_meta("entity", "")),
			(get_parent() as Node2D).global_position,
			_bounds
		)


class Blink:
	extends Node
	## Purely cosmetic emissive pulse — a designer telegraph meaning "this is the
	## interactive one". 7 of its 9 wall uses are in world 2. It has NO effect on
	## collision, breakability, physics, portal eligibility or scoring, so it must never
	## be allowed to gate anything.

	var _t: float = 0.0

	func _process(delta: float) -> void:
		_t += delta
		var parent: Node2D = get_parent() as Node2D
		# A burning crate owns `modulate`; do not fight it for the same property.
		var burning: Node = parent.get_node_or_null("Burnable")
		if burning != null and burning.has_method("is_burning") and burning.is_burning():
			return
		var pulse: float = (cos(_t * 4.0) + 1.0) * 0.5
		parent.modulate = Color.WHITE.lerp(Color(1.4, 1.4, 1.2), pulse * 0.5)
