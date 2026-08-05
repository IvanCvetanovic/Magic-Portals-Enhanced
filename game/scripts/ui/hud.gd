class_name Hud
extends CanvasLayer
## In-game overlay: level label, crystal count, portal budget, and the touch controls.
##
## Built in code rather than authored as a scene because every size comes from JSON
## (CLAUDE.md rule 1) — most importantly the ≥96 px touch-target floor (rule 4), which a
## hand-placed `.tscn` would quietly drift away from.
##
## THE ON-SCREEN PAD IS TWO BUTTONS, LEFT AND RIGHT. That is not a simplification: the
## original's `SCREEN_PAD_BUTTON` enum decodes to exactly `NO_SCREEN_BUTTON`,
## `LEFT_SCREEN_BUTTON`, `RIGHT_SCREEN_BUTTON` (`tools/asbc/`), and the owner confirms it.
## There is no jump and no action button — portals are placed by tapping the world.
##
## The pad publishes through EventBus exactly like a physical key would, so gameplay
## cannot tell touch from keyboard (rule 4).

signal restart_pressed
signal menu_pressed

var _level_label: Label
var _crystal_label: Label
var _portal_label: Label
var _left_button: Button
var _right_button: Button
var _pad_direction: float = 0.0


func _ready() -> void:
	layer = 10
	var touch_size: float = float(
		DataLoader.get_value("game", ["input", "min_touch_target_px"], 96)
	)
	_build_status_bar()
	_build_pad(touch_size)
	_build_buttons(touch_size)

	EventBus.level_loaded.connect(_on_level_loaded)
	EventBus.portal_placed.connect(_on_portal_changed)
	EventBus.portals_consumed.connect(_on_portals_consumed)


func _build_status_bar() -> void:
	var bar: HBoxContainer = HBoxContainer.new()
	bar.set_anchors_preset(Control.PRESET_TOP_WIDE)
	bar.offset_left = 12
	bar.offset_top = 8
	bar.offset_right = -12
	bar.add_theme_constant_override("separation", 24)
	add_child(bar)

	_level_label = _make_label("")
	_crystal_label = _make_label("")
	_portal_label = _make_label("")
	bar.add_child(_level_label)
	bar.add_child(_crystal_label)
	bar.add_child(_portal_label)


func _make_label(text: String) -> Label:
	var label: Label = Label.new()
	label.text = text
	label.add_theme_font_size_override("font_size", 20)
	# The original's levels are dark; an outline keeps the HUD readable over any of them
	# without needing a panel behind it.
	label.add_theme_color_override("font_outline_color", Color.BLACK)
	label.add_theme_constant_override("outline_size", 4)
	return label


func _build_pad(touch_size: float) -> void:
	_left_button = _make_pad_button("◀", touch_size)
	_left_button.set_anchors_preset(Control.PRESET_BOTTOM_LEFT)
	_left_button.offset_left = 16
	_left_button.offset_top = -(touch_size + 16)
	_left_button.offset_right = 16 + touch_size
	_left_button.offset_bottom = -16
	add_child(_left_button)

	_right_button = _make_pad_button("▶", touch_size)
	_right_button.set_anchors_preset(Control.PRESET_BOTTOM_LEFT)
	_right_button.offset_left = 16 + touch_size + 12
	_right_button.offset_top = -(touch_size + 16)
	_right_button.offset_right = 16 + touch_size * 2 + 12
	_right_button.offset_bottom = -16
	add_child(_right_button)

	# button_down/up rather than `pressed`: movement is a held state, not an event.
	_left_button.button_down.connect(func() -> void: _set_pad(-1.0))
	_left_button.button_up.connect(func() -> void: _set_pad(0.0))
	_right_button.button_down.connect(func() -> void: _set_pad(1.0))
	_right_button.button_up.connect(func() -> void: _set_pad(0.0))


func _make_pad_button(text: String, touch_size: float) -> Button:
	var button: Button = Button.new()
	button.text = text
	button.custom_minimum_size = Vector2(touch_size, touch_size)
	button.focus_mode = Control.FOCUS_NONE
	# Without this the pad would eat the tap that places a portal, and the player could
	# never build one over the bottom-left of the level.
	button.mouse_filter = Control.MOUSE_FILTER_STOP
	return button


func _build_buttons(touch_size: float) -> void:
	var box: VBoxContainer = VBoxContainer.new()
	box.set_anchors_preset(Control.PRESET_TOP_RIGHT)
	box.offset_left = -(touch_size + 16)
	box.offset_top = 44
	box.offset_right = -16
	add_child(box)

	var restart: Button = Button.new()
	restart.text = "↻"
	restart.custom_minimum_size = Vector2(touch_size, touch_size * 0.6)
	restart.focus_mode = Control.FOCUS_NONE
	restart.pressed.connect(func() -> void: restart_pressed.emit())
	box.add_child(restart)

	var menu: Button = Button.new()
	menu.text = "☰"
	menu.custom_minimum_size = Vector2(touch_size, touch_size * 0.6)
	menu.focus_mode = Control.FOCUS_NONE
	menu.pressed.connect(func() -> void: menu_pressed.emit())
	box.add_child(menu)


func _set_pad(direction: float) -> void:
	if is_equal_approx(direction, _pad_direction):
		return
	_pad_direction = direction
	# Same signal the keyboard path emits, so the player has one code path (rule 4).
	EventBus.move_intent_changed.emit(Vector2(direction, 0.0))


func _on_level_loaded(world: int, level: int) -> void:
	# 1-based for display only; every index in the codebase is 0-based to match the
	# original's own on-disk numbering.
	_level_label.text = "%d-%d" % [world + 1, level + 1]


## Called by Main after LevelRuntime has bound, since the HUD cannot see the level itself.
func set_level_stats(crystals: int, portal_budget: int) -> void:
	_crystal_label.text = "◆ %d" % crystals
	_portal_label.text = "◉ %d" % portal_budget
	_crystal_label.visible = crystals > 0
	_portal_label.visible = portal_budget > 0


func set_crystals(remaining: int) -> void:
	_crystal_label.text = "◆ %d" % remaining


func _on_portal_changed(_world_position: Vector2, active_count: int) -> void:
	_portal_label.text = "◉ %d" % active_count


func _on_portals_consumed(active_count: int) -> void:
	_portal_label.text = "◉ %d" % active_count
