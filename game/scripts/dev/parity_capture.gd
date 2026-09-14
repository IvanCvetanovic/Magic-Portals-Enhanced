class_name ParityCapture
extends Node
## Dev-only: renders one screen of the remake and writes it to disk, so it can be compared
## side by side with a capture of the original running in the Android emulator.
##
## Driven entirely by user args after `--`, so a normal launch never constructs it:
##
##     tools\godot.bat --path game --resolution 1280x720 -- --parity-level=1-1
##         --parity-out=C:/abs/path/1-1.png --parity-delay=6
##
## Must run WITH a window: `--headless` has no renderer, so there is no frame to save.
## Level ids are 1-based ("1-1") to match what both games display; everything inside the
## codebase stays 0-based.

const ARG_LEVEL: String = "--parity-level="
const ARG_OUT: String = "--parity-out="
const ARG_DELAY: String = "--parity-delay="

var _start_level: Callable
var _level_spec: String = ""
var _out_path: String = ""
var _delay_s: float = 0.0


## True when the process was launched to capture a parity frame.
static func requested() -> bool:
	for arg: String in OS.get_cmdline_user_args():
		if arg.begins_with(ARG_OUT):
			return true
	return false


## `start_level` is Main's (world, level) -> void entry point, passed in rather than reached
## for, so this dev tool cannot drift into depending on Main's internals.
func _init(start_level: Callable) -> void:
	_start_level = start_level
	for arg: String in OS.get_cmdline_user_args():
		if arg.begins_with(ARG_LEVEL):
			_level_spec = arg.substr(ARG_LEVEL.length())
		elif arg.begins_with(ARG_OUT):
			_out_path = arg.substr(ARG_OUT.length())
		elif arg.begins_with(ARG_DELAY):
			_delay_s = arg.substr(ARG_DELAY.length()).to_float()


func _ready() -> void:
	_run.call_deferred()


func _run() -> void:
	if not _level_spec.is_empty():
		var parts: PackedStringArray = _level_spec.split("-")
		if parts.size() != 2 or not parts[0].is_valid_int() or not parts[1].is_valid_int():
			_fail("bad --parity-level '%s', expected W-L such as 1-1" % _level_spec)
			return
		var world: int = parts[0].to_int() - 1
		var level: int = parts[1].to_int() - 1
		# In memory only: capture must reach any level without touching the player's save.
		GameState.persist_enabled = false
		if not GameState.is_unlocked(world, level):
			GameState.unlocked_world = world
			GameState.unlocked_level = level
		_start_level.call(world, level)

	if _delay_s > 0.0:
		await get_tree().create_timer(_delay_s).timeout
	await RenderingServer.frame_post_draw

	var image: Image = get_viewport().get_texture().get_image()
	var err: Error = image.save_png(_out_path)
	if err != OK:
		_fail("could not write %s (error %d)" % [_out_path, err])
		return
	print("parity: wrote %s (%dx%d)" % [_out_path, image.get_width(), image.get_height()])
	# Quitting inside frame_post_draw leaves the viewport texture referenced at exit.
	await get_tree().process_frame
	get_tree().quit(0)


func _fail(message: String) -> void:
	push_error("ParityCapture: " + message)
	get_tree().quit(1)
