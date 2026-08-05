class_name InputController
extends Node
## Translates raw input into semantic intents. CLAUDE.md rule 4.
##
## Gameplay code never reads InputEvent. Touch and mouse converge HERE and nowhere else,
## so every downstream system has exactly one code path to reason about and desktop
## playtesting exercises the same logic the phone will.
##
## Deliberately NOT an autoload: CLAUDE.md rule 3 caps autoloads at three (EventBus,
## GameState, DataLoader) and says not to add a fourth without asking. This is a normal
## node in the running scene, and it publishes through EventBus.
##
## `project.godot` disables BOTH pointer-emulation flags, so on Android we see touch only
## and on desktop mouse only — no event is ever processed twice. That is the reason this
## class can treat the two symmetrically without de-duplicating.

## Semantic actions the game understands. No magic strings (CLAUDE.md rule 7).
enum Intent {
	NONE,
	PRIMARY,  ## tap / left click — the main "do the thing" action
	CANCEL,  ## back / right click / escape
	RESTART,  ## retry the level, must be instant (MILESTONE_1 §4)
}

## Pointer travel beyond this many pixels turns a press into a drag, not a tap.
var _tap_max_travel_px: float = 24.0
## Presses held longer than this stop counting as taps.
var _tap_max_duration_s: float = 0.25

var _press_position: Vector2 = Vector2.ZERO
var _press_time_s: float = 0.0
var _press_index: int = -1
var _is_pressed: bool = false
var _move_direction: Vector2 = Vector2.ZERO


func _ready() -> void:
	# Tunables come from JSON, never from literals (CLAUDE.md rule 1). The fields above
	# are fallbacks for when DataLoader has not loaded, not the source of truth.
	_tap_max_travel_px = float(DataLoader.get_value("game", ["input", "tap_max_travel_px"], 24.0))
	_tap_max_duration_s = float(DataLoader.get_value("game", ["input", "tap_max_duration_s"], 0.25))


func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventScreenTouch:
		var touch: InputEventScreenTouch = event
		_handle_press(Intent.PRIMARY, touch.position, touch.pressed, touch.index)
	elif event is InputEventMouseButton:
		var click: InputEventMouseButton = event
		var intent: Intent = _mouse_intent(click.button_index)
		if intent != Intent.NONE:
			# Mouse buttons share the touch path by using the negated button index as a
			# pointer id, so left and right clicks cannot clobber each other's press state.
			_handle_press(intent, click.position, click.pressed, -click.button_index)
	elif event is InputEventKey:
		_handle_key(event)

	_update_move_direction()


func _mouse_intent(button_index: int) -> Intent:
	match button_index:
		MOUSE_BUTTON_LEFT:
			return Intent.PRIMARY
		MOUSE_BUTTON_RIGHT:
			return Intent.CANCEL
		_:
			return Intent.NONE


func _handle_key(event: InputEventKey) -> void:
	if event.echo:
		return
	var intent: Intent = Intent.NONE
	match event.keycode:
		KEY_ESCAPE:
			intent = Intent.CANCEL
		KEY_R:
			intent = Intent.RESTART
		_:
			return
	# Keyboard has no position. Report the pointer's last known spot rather than a
	# meaningless zero, so a handler can treat every intent uniformly.
	if event.pressed:
		EventBus.intent_pressed.emit(intent, _press_position)
	else:
		EventBus.intent_released.emit(intent, _press_position, 0.0)


func _handle_press(intent: Intent, position: Vector2, pressed: bool, pointer_id: int) -> void:
	if pressed:
		# First pointer wins. Ignoring extra fingers keeps multi-touch from producing
		# interleaved press/release pairs that a single-pointer game would misread.
		if _is_pressed:
			return
		_is_pressed = true
		_press_index = pointer_id
		_press_position = position
		_press_time_s = _now_s()
		EventBus.intent_pressed.emit(intent, position)
		return

	if not _is_pressed or pointer_id != _press_index:
		return
	_is_pressed = false
	_press_index = -1
	EventBus.intent_released.emit(intent, position, _now_s() - _press_time_s)


## True when the just-released press was a tap rather than a drag or a hold.
##
## Both thresholds matter: a slow press that never moves is a hold, and a fast one that
## travels is a swipe. Callers get this instead of re-deriving it from raw positions.
func was_tap(release_position: Vector2, duration_s: float) -> bool:
	return (
		duration_s <= _tap_max_duration_s
		and release_position.distance_to(_press_position) <= _tap_max_travel_px
	)


func _update_move_direction() -> void:
	# Keyboard movement exists for desktop playtesting only; the shipped touch control
	# scheme is unresolved and depends on §7.6 video. Named actions are not used because
	# project.godot defines none yet — adding an InputMap here would be a second source
	# of truth for bindings.
	var direction: Vector2 = Vector2(
		float(Input.is_key_pressed(KEY_D) or Input.is_key_pressed(KEY_RIGHT))
		- float(Input.is_key_pressed(KEY_A) or Input.is_key_pressed(KEY_LEFT)),
		float(Input.is_key_pressed(KEY_S) or Input.is_key_pressed(KEY_DOWN))
		- float(Input.is_key_pressed(KEY_W) or Input.is_key_pressed(KEY_UP))
	)
	if direction != Vector2.ZERO:
		direction = direction.normalized()
	if direction != _move_direction:
		_move_direction = direction
		EventBus.move_intent_changed.emit(direction)


func _now_s() -> float:
	return float(Time.get_ticks_msec()) / 1000.0
