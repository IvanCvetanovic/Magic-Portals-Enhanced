extends Node2D
## Application root: menu ⇄ gameplay, and the wiring between input, level and progression.
##
## Owns the long-lived systems (input, audio, portals, level manager, HUD, menus) and a
## `Levels` container that the current level is swapped into. Everything spawned during
## play parents into the level, so a retry frees it — nothing accumulates in here.
##
## Controls follow the original: two movement buttons and taps to place portals. There is
## no jump. See `docs/original-gameplay.md`.

enum Screen { MENU, PLAYING }

## Camera2D's own default for an unset limit.
const CAMERA_UNBOUNDED: int = 10000000

@onready var _levels: Node2D = $Levels

var _input: InputController
var _audio: AudioManager
var _manager: LevelManager
var _runtime: LevelRuntime
var _portals: PortalSystem
var _hud: Hud
var _menu: MenuScreen
var _camera: Camera2D
var _player: Player
var _screen: Screen = Screen.MENU


func _ready() -> void:
	if not DataLoader.is_ready:
		push_warning("Main: data not ready — %s" % DataLoader.missing_reason)
		return

	_input = InputController.new()
	_input.name = "InputController"
	add_child(_input)

	_audio = AudioManager.new()
	_audio.name = "AudioManager"
	add_child(_audio)

	_runtime = LevelRuntime.new()
	_runtime.name = "LevelRuntime"
	add_child(_runtime)

	_portals = PortalSystem.new()
	_portals.name = "PortalSystem"
	add_child(_portals)

	_manager = LevelManager.new(_levels)
	_manager.name = "LevelManager"
	add_child(_manager)

	_camera = Camera2D.new()
	_camera.name = "Camera"
	add_child(_camera)

	_hud = Hud.new()
	_hud.name = "Hud"
	add_child(_hud)

	_menu = MenuScreen.new()
	_menu.name = "MenuScreen"
	add_child(_menu)

	_manager.level_instantiated.connect(_on_level_instantiated)
	_runtime.exit_reached.connect(_on_exit_reached)
	_runtime.player_died.connect(_on_player_died)
	_runtime.crystal_collected.connect(_on_crystal_collected)
	_hud.restart_pressed.connect(func() -> void: _manager.restart_level())
	_hud.menu_pressed.connect(_open_menu)
	_menu.play_pressed.connect(_start_level)
	_menu.closed.connect(_close_menu)
	EventBus.intent_pressed.connect(_on_intent_pressed)

	_show(Screen.MENU)

	if ParityCapture.requested():
		add_child(ParityCapture.new(_start_level))


func _show(screen: Screen) -> void:
	_screen = screen
	_menu.visible = screen == Screen.MENU
	_hud.visible = screen == Screen.PLAYING
	# Pausing the tree would also freeze the menu's own input, so gameplay is suspended by
	# suspending the things that act: physics bodies and the portal system.
	_levels.process_mode = (
		Node.PROCESS_MODE_DISABLED if screen == Screen.MENU else Node.PROCESS_MODE_INHERIT
	)
	_portals.process_mode = _levels.process_mode


func _start_level(world: int, level: int) -> void:
	if not _manager.load_level(world, level):
		push_warning("Main: could not load %d-%d" % [world + 1, level + 1])
		return
	_show(Screen.PLAYING)



func _open_menu() -> void:
	_menu.show_level_select(_manager.current_world)
	_show(Screen.MENU)


func _close_menu() -> void:
	# Only resumable if a level is actually loaded; from a cold start there is nothing
	# behind the menu to go back to.
	if _manager.current_root != null and is_instance_valid(_manager.current_root):
		_show(Screen.PLAYING)


func _on_level_instantiated(root: Node) -> void:
	_runtime.bind(root)
	_portals.bind(_runtime)

	# Rebuilt per level rather than repositioned, so a retry cannot inherit velocity, a
	# death flag or buffered input from the previous attempt.
	_player = Player.new()
	_player.name = "Player"
	_player.position = _runtime.spawn_position
	_player.gravity_enabled = not _runtime.flag("no_gravity")
	_player.died.connect(_on_player_died)
	root.add_child(_player)
	_portals.set_player(_player)

	_apply_camera_bounds()
	# The original opens with the view on the player, clamped to the level — not on the
	# camera_start marker, which 1-1 places at (651, 0) while showing (0, 0). The limits
	# do the clamping, so starting on the player is enough.
	_camera.position = _player.position
	_camera.make_current()
	_camera.reset_smoothing()
	_hud.set_level_stats(_runtime.crystals_total, _runtime.max_portals)
	_apply_level_properties(root)


func _process(delta: float) -> void:
	if _screen != Screen.PLAYING or _player == null or not is_instance_valid(_player):
		return
	# Per-frame interpolation is fine; rule 6 forbids per-frame *scanning*. The lag value
	# is a GUESS (portals.json → camera.follow_lag_s).
	var lag: float = float(DataLoader.get_value("portals", ["camera", "follow_lag_s"], 0.12))
	_camera.position = _camera.position.lerp(
		_player.position, clampf(delta / maxf(lag, 0.001), 0.0, 1.0)
	)


func _apply_camera_bounds() -> void:
	# Camera2D limits clamp the visible rectangle natively, which is what the original's
	# clampCameraPos does: the view never shows past the origin or the `max` marker.
	var bounds_min: Array = DataLoader.get_value("portals", ["camera", "bounds_min"], [0, 0])
	_camera.limit_left = int(bounds_min[0])
	_camera.limit_top = int(bounds_min[1])
	if _runtime.bounds_max == Vector2.ZERO:
		push_warning("Main: level has no `max` marker; the camera is unbounded right and down")
		_camera.limit_right = CAMERA_UNBOUNDED
		_camera.limit_bottom = CAMERA_UNBOUNDED
		return
	_camera.limit_right = int(_runtime.bounds_max.x)
	_camera.limit_bottom = int(_runtime.bounds_max.y)


func _on_intent_pressed(intent: int, _screen_position: Vector2) -> void:
	if _screen != Screen.PLAYING:
		return
	match intent:
		InputController.Intent.RESTART:
			_manager.restart_level()
		InputController.Intent.CANCEL:
			_open_menu()


func _on_crystal_collected(remaining: int) -> void:
	_hud.set_crystals(remaining)
	_audio.play(&"crystal")


func _on_player_died(reason: StringName) -> void:
	_manager.fail_level(reason)
	# MILESTONE_1 §4: retry is instant, no load screen.
	_manager.restart_level()


func _on_exit_reached() -> void:
	if not _runtime.exit_is_open():
		return
	_audio.play(&"door_open")
	_manager.complete_level(_portals.portals_used)
	if bool(DataLoader.get_value("game", ["progression", "advance_on_complete"], true)):
		if not _manager.advance_level():
			_open_menu()


func _unhandled_key_input(event: InputEvent) -> void:
	var key: InputEventKey = event as InputEventKey
	if key == null or not key.pressed or key.echo or _screen != Screen.PLAYING:
		return
	# Dev shortcut, deliberately not a player intent: it exists so the level pipeline can
	# be walked before every level is finishable.
	if key.keycode == KEY_N and not _manager.advance_level():
		_open_menu()


func _apply_level_properties(root: Node) -> void:
	# `darkest` marks chapter 4's unlit levels — the ones the "Alone in the dark"
	# achievements refer to. Modelled as a CanvasModulate over the level rather than real
	# lighting: the original's per-entity emissive data is converted but not yet used, and
	# a flat darkening is the honest approximation until it is.
	if _runtime.flag("darkest"):
		var dark: CanvasModulate = CanvasModulate.new()
		dark.name = "Darkest"
		dark.color = Color(0.28, 0.28, 0.34)
		root.add_child(dark)

	# `no_music` is a real per-level flag; 4-1, 4-19 and 4-32 set it.
	if _runtime.flag("no_music"):
		_audio.stop_music()
	else:
		_audio.play_music(StringName("music_world%d" % _manager.current_world))
