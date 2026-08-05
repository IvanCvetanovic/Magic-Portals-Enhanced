class_name Player
extends CharacterBody2D
## The player character.
##
## ⚠ EVERY MOVEMENT NUMBER HERE COMES FROM `player.json`, AND EVERY ONE OF THOSE IS A
## GUESS. Nothing has been measured against the original. This is a playable placeholder
## so the level pipeline can be exercised end to end — it is explicitly NOT a claim about
## how Magic Portals feels. See `player.json` → `_about`, and MILESTONE_1.md §3.
##
## What IS established (from identifiers in the compiled AngelScript, so structure not
## numbers): the character is player-driven via a `ScreenPad` with a
## `DIRECTION{MOVING_UP,DOWN,LEFT,RIGHT}` enum — it does not auto-walk — and ground
## contact is a sensor entity (`char_feet.ent`) rather than a raycast.
##
## Input arrives only through EventBus (CLAUDE.md rule 4). This class never touches
## InputEvent, so the touch and desktop paths are identical here by construction.

signal died(reason: StringName)

var _speed: float = 160.0
var _accel: float = 1400.0
var _friction: float = 1800.0
var _air_control: float = 0.65
var _gravity: float = 1200.0
var _max_fall: float = 900.0
var _jump_enabled: bool = true
var _jump_velocity: float = -420.0
var _coyote_time: float = 0.0
var _jump_buffer: float = 0.0

var _move_dir: Vector2 = Vector2.ZERO
var _coyote_left: float = 0.0
var _buffered_jump_left: float = 0.0
var _is_dead: bool = false

## Zero-g levels. Chapter 4 levels 2-19 set `no_gravity` on their `properties` entity —
## that is the space chapter, and applying gravity there makes them unplayable (the player
## simply falls out of every one).
var gravity_enabled: bool = true


func _ready() -> void:
	add_to_group("player")
	_load_tuning()
	_build_body()
	EventBus.move_intent_changed.connect(_on_move_intent_changed)
	# NOT connected to intent_pressed. A tap/click is PORTAL PLACEMENT (OWNER: movement is
	# two buttons, portals are placed by tapping) and belongs to PortalSystem. Binding
	# jump to PRIMARY here would fire a jump on every portal the player places.


func _load_tuning() -> void:
	_speed = float(DataLoader.get_value("player", ["movement", "walk_speed_px_s"], 160.0))
	_accel = float(DataLoader.get_value("player", ["movement", "acceleration_px_s2"], 1400.0))
	_friction = float(DataLoader.get_value("player", ["movement", "friction_px_s2"], 1800.0))
	_air_control = float(DataLoader.get_value("player", ["movement", "air_control"], 0.65))
	_gravity = float(DataLoader.get_value("player", ["gravity", "gravity_px_s2"], 1200.0))
	_max_fall = float(DataLoader.get_value("player", ["gravity", "max_fall_speed_px_s"], 900.0))
	_jump_enabled = bool(DataLoader.get_value("player", ["jump", "enabled"], true))
	_jump_velocity = float(DataLoader.get_value("player", ["jump", "jump_velocity_px_s"], -420.0))
	# Both default to 0 because a sweep of the original binary for coyote/buffer
	# identifiers found NOTHING — weak but real evidence there is no forgiveness system.
	_coyote_time = float(DataLoader.get_value("player", ["jump", "coyote_time_s"], 0.0))
	_jump_buffer = float(DataLoader.get_value("player", ["jump", "jump_buffer_s"], 0.0))


func _build_body() -> void:
	# Built in code rather than authored as a scene: CLAUDE.md rule 5 wants entities to be
	# generic-plus-data, and the capsule size is a tunable in player.json.
	var shape: CollisionShape2D = CollisionShape2D.new()
	var capsule: CapsuleShape2D = CapsuleShape2D.new()
	capsule.radius = float(DataLoader.get_value("player", ["body", "width_px"], 20.0)) * 0.5
	capsule.height = float(DataLoader.get_value("player", ["body", "height_px"], 44.0))
	shape.shape = capsule
	add_child(shape)


func _on_move_intent_changed(direction: Vector2) -> void:
	_move_dir = direction


## Request a jump. Nothing calls this in normal play — see `_ready` and `player.json`:
## the original almost certainly has no jump, so this exists only for desktop debugging.
func request_jump() -> void:
	_buffered_jump_left = maxf(_jump_buffer, 0.0)
	_try_jump()


func _physics_process(delta: float) -> void:
	if _is_dead:
		return

	var on_floor: bool = is_on_floor()
	if on_floor:
		_coyote_left = _coyote_time
	else:
		_coyote_left = maxf(0.0, _coyote_left - delta)
		if gravity_enabled:
			velocity.y = minf(velocity.y + _gravity * delta, _max_fall)

	var target: float = _move_dir.x * _speed
	var rate: float = _accel if not is_zero_approx(target) else _friction
	if not on_floor:
		rate *= _air_control
	velocity.x = move_toward(velocity.x, target, rate * delta)

	if _buffered_jump_left > 0.0:
		_buffered_jump_left = maxf(0.0, _buffered_jump_left - delta)
		_try_jump()

	move_and_slide()


func _try_jump() -> void:
	if not _jump_enabled or _is_dead:
		return
	# With coyote_time at 0 this is exactly "must be on the floor", which is the intended
	# default — the forgiveness window only opens if the data says the original has one.
	if is_on_floor() or _coyote_left > 0.0:
		velocity.y = _jump_velocity
		_coyote_left = 0.0
		_buffered_jump_left = 0.0


## Kill the player. Idempotent — a hazard trigger can fire more than once per contact.
func kill(reason: StringName = &"unspecified") -> void:
	if _is_dead:
		return
	_is_dead = true
	velocity = Vector2.ZERO
	died.emit(reason)


func teleport_to(target: Vector2, exit_velocity: Vector2) -> void:
	global_position = target
	velocity = exit_velocity
