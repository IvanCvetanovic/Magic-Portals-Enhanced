class_name MenuScreen
extends CanvasLayer
## Main menu and level select.
##
## Both are button grids, so they share one script and differ by state. The original has
## `PortalMainMenu` and `PortalLevelSelector` plus four `level_select0..3.esc` scenes —
## one per world — which is where the four-world tab layout comes from.
##
## Locking is read from `GameState`, which models the original's progression exactly: a
## single linear high-water mark, not per-level stars (`docs/original-gameplay.md` §3).
##
## Built in code for the same reason as the HUD: touch-target sizes come from JSON and a
## hand-authored `.tscn` drifts away from the ≥96 px floor.

signal play_pressed(world: int, level: int)
signal closed

enum Mode { MAIN, LEVEL_SELECT }

const WORLD_COUNT: int = 4

var _mode: Mode = Mode.MAIN
var _selected_world: int = 0
var _root: Control
var _touch_size: float = 96.0


func _ready() -> void:
	layer = 20
	_touch_size = float(DataLoader.get_value("game", ["input", "min_touch_target_px"], 96))
	_root = Control.new()
	_root.set_anchors_preset(Control.PRESET_FULL_RECT)
	# Without this the menu lets clicks through and the player places portals behind it.
	_root.mouse_filter = Control.MOUSE_FILTER_STOP
	add_child(_root)
	show_main()


func show_main() -> void:
	_mode = Mode.MAIN
	_rebuild()


func show_level_select(world: int = -1) -> void:
	_mode = Mode.LEVEL_SELECT
	# Default to the world the player has actually reached rather than always world 1.
	_selected_world = GameState.unlocked_world if world < 0 else world
	_rebuild()


func _rebuild() -> void:
	for child: Node in _root.get_children():
		child.queue_free()

	var backdrop: ColorRect = ColorRect.new()
	backdrop.color = Color(0.05, 0.04, 0.08, 0.92)
	backdrop.set_anchors_preset(Control.PRESET_FULL_RECT)
	backdrop.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_root.add_child(backdrop)

	var column: VBoxContainer = VBoxContainer.new()
	column.set_anchors_preset(Control.PRESET_FULL_RECT)
	column.offset_left = 24
	column.offset_top = 24
	column.offset_right = -24
	column.offset_bottom = -24
	column.add_theme_constant_override("separation", 12)
	_root.add_child(column)

	if _mode == Mode.MAIN:
		_build_main(column)
	else:
		_build_level_select(column)


func _title(text: String, size: int = 40) -> Label:
	var label: Label = Label.new()
	label.text = text
	label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	label.add_theme_font_size_override("font_size", size)
	return label


func _big_button(text: String) -> Button:
	var button: Button = Button.new()
	button.text = text
	button.custom_minimum_size = Vector2(0, _touch_size)
	button.focus_mode = Control.FOCUS_NONE
	return button


func _build_main(column: VBoxContainer) -> void:
	column.add_child(_title("MAGIC PORTALS"))
	column.add_child(_title("remake", 18))

	var spacer: Control = Control.new()
	spacer.custom_minimum_size = Vector2(0, 24)
	column.add_child(spacer)

	# "Continue" resumes at the frontier, which for a fresh save is level 1-1 — so this
	# doubles as "new game" without needing a second button or a confirmation prompt.
	var resume: Button = _big_button(
		"CONTINUE  %d-%d" % [GameState.unlocked_world + 1, GameState.unlocked_level + 1]
	)
	resume.pressed.connect(
		func() -> void: play_pressed.emit(GameState.unlocked_world, GameState.unlocked_level)
	)
	column.add_child(resume)

	var select: Button = _big_button("LEVEL SELECT")
	select.pressed.connect(func() -> void: show_level_select())
	column.add_child(select)

	var quit: Button = _big_button("QUIT")
	quit.pressed.connect(func() -> void: get_tree().quit())
	column.add_child(quit)


func _build_level_select(column: VBoxContainer) -> void:
	column.add_child(_title("CHAPTER %d" % (_selected_world + 1), 28))

	var tabs: HBoxContainer = HBoxContainer.new()
	tabs.alignment = BoxContainer.ALIGNMENT_CENTER
	tabs.add_theme_constant_override("separation", 8)
	column.add_child(tabs)
	for world: int in range(WORLD_COUNT):
		var tab: Button = Button.new()
		tab.text = str(world + 1)
		tab.custom_minimum_size = Vector2(_touch_size * 0.6, _touch_size * 0.6)
		tab.focus_mode = Control.FOCUS_NONE
		tab.toggle_mode = true
		tab.button_pressed = world == _selected_world
		# A whole chapter is locked until progression reaches it.
		tab.disabled = not GameState.is_unlocked(world, 0)
		tab.pressed.connect(func() -> void: show_level_select(world))
		tabs.add_child(tab)

	var grid: GridContainer = GridContainer.new()
	grid.columns = 8
	grid.add_theme_constant_override("h_separation", 6)
	grid.add_theme_constant_override("v_separation", 6)
	column.add_child(grid)

	var total: int = DataLoader.level_count(_selected_world)
	if total == 0:
		column.add_child(
			_title("chapter not converted — run the converter for world %d" % _selected_world, 16)
		)
	for level: int in range(total):
		grid.add_child(_level_button(_selected_world, level))

	var back: Button = _big_button("BACK")
	back.pressed.connect(func() -> void: show_main())
	column.add_child(back)


func _level_button(world: int, level: int) -> Button:
	var button: Button = Button.new()
	var unlocked: bool = GameState.is_unlocked(world, level)
	var key: String = "%d:%d" % [world, level]
	var best: Variant = GameState.best_portals.get(key)

	if not unlocked:
		button.text = "🔒"
	elif best == null:
		button.text = str(level + 1)
	else:
		# A medal marks a cleared level. Gold is awarded for finishing at or under the
		# level's golden score, which is a MAXIMUM PORTAL COUNT — lower is better, and a
		# threshold of 0 genuinely means "use no portals".
		var threshold: int = DataLoader.golden_score(world, level)
		var gold: bool = threshold >= 0 and int(best) <= threshold
		button.text = "%d %s" % [level + 1, "★" if gold else "✓"]

	button.custom_minimum_size = Vector2(_touch_size * 0.75, _touch_size * 0.6)
	button.focus_mode = Control.FOCUS_NONE
	button.disabled = not unlocked
	button.pressed.connect(func() -> void: play_pressed.emit(world, level))
	return button


func _unhandled_key_input(event: InputEvent) -> void:
	var key: InputEventKey = event as InputEventKey
	if key == null or not key.pressed or key.echo:
		return
	if key.keycode == KEY_ESCAPE:
		if _mode == Mode.LEVEL_SELECT:
			show_main()
		else:
			closed.emit()
		get_viewport().set_input_as_handled()
