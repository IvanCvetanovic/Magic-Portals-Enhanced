class_name LevelManager
extends Node
## Loads levels and owns the complete / fail / retry / next cycle. MILESTONE_1 §4.
##
## RETRY MUST BE INSTANT — no load screen between attempts (§4). Puzzle-platformers live
## or die on retry friction. Two things make that true here:
##
##   * the PackedScene for the current level is cached, so a retry is an instantiate, not
##     a disk load; and
##   * the swap happens inside one frame, with no fade or transition in the path.
##
## Anything added to the retry path later must preserve both. If a transition is ever
## wanted, it belongs on level *advance*, never on retry.
##
## Levels come from converter output under `res://assets/levels/`, which is gitignored —
## a fresh clone has none (see README.md). Failing to load is therefore an ordinary
## first-run state, and it is reported rather than treated as a crash.

## Emitted locally as well as on EventBus; a direct parent usually wants the node itself,
## which is not something a global signal should be shipping around.
signal level_instantiated(root: Node)

var current_world: int = 0
var current_level: int = 0
var current_root: Node = null

## Cached so retry never touches the disk.
var _packed: PackedScene = null
var _container: Node = null


func _init(container: Node = null) -> void:
	_container = container


## Set the node that instantiated levels are parented to.
##
## Must be called before load_level() if the manager was not constructed with one.
## There is deliberately NO fallback to `self`: a silent fallback would parent levels to
## the manager instead of the intended container, producing a wrong scene tree with no
## error — and `_init` args are not passed when Godot instantiates a node from a `.tscn`,
## so that null path is genuinely reachable.
func set_container(container: Node) -> void:
	_container = container


func _ready() -> void:
	if _container == null:
		push_error(
			"LevelManager has no container. Construct it as LevelManager.new(container) "
			+ "or call set_container() before load_level()."
		)
	EventBus.level_load_requested.connect(_on_level_load_requested)


func _on_level_load_requested(world: int, level: int) -> void:
	load_level(world, level)


## Load and instantiate a level. Returns false and explains itself on failure.
func load_level(world: int, level: int) -> bool:
	if _container == null:
		push_error("LevelManager: no container set; refusing to load %d-%d" % [world + 1, level + 1])
		return false
	if not GameState.is_unlocked(world, level):
		push_warning("LevelManager: %d-%d is locked" % [world + 1, level + 1])
		return false

	var path: String = DataLoader.level_scene_path(world, level)
	if path.is_empty():
		push_warning(
			"LevelManager: no scene for %d-%d. %s" % [world + 1, level + 1, DataLoader.missing_reason]
		)
		return false
	if not ResourceLoader.exists(path):
		push_warning("LevelManager: manifest lists %s but it does not exist" % path)
		return false

	var packed: PackedScene = load(path) as PackedScene
	if packed == null:
		push_warning("LevelManager: %s is not a PackedScene" % path)
		return false

	_packed = packed
	current_world = world
	current_level = level
	GameState.current_world = world
	GameState.current_level = level

	_swap_in(packed)
	EventBus.level_loaded.emit(world, level)
	return true


## Re-instantiate the current level from cache. This is the hot path — keep it cheap.
func restart_level() -> bool:
	if _packed == null:
		return load_level(current_world, current_level)
	_swap_in(_packed)
	EventBus.level_restarted.emit(current_world, current_level)
	return true


func _swap_in(packed: PackedScene) -> void:
	if current_root != null and is_instance_valid(current_root):
		# free() not queue_free(): the replacement is added THIS frame, and deferred
		# deletion would leave both levels alive together — double physics bodies, double
		# input handlers, and a one-frame flicker on every retry.
		_container.remove_child(current_root)
		current_root.free()
	current_root = packed.instantiate()
	_container.add_child(current_root)
	level_instantiated.emit(current_root)


## Report the level cleared. `portals_used` is what the golden score is measured against.
func complete_level(portals_used: int) -> void:
	GameState.mark_completed(current_world, current_level, portals_used)
	EventBus.level_completed.emit(current_world, current_level, portals_used)
	if GameState.is_chapter_complete(current_world):
		EventBus.chapter_completed.emit(current_world)


func fail_level(reason: StringName = &"unspecified") -> void:
	EventBus.level_failed.emit(current_world, current_level, reason)


## Advance to the next level. False when the chapter has no next level — the caller
## decides what a finished chapter means, because that is a meta-layer concern.
func advance_level() -> bool:
	var next: int = current_level + 1
	if next >= DataLoader.level_count(current_world):
		return false
	return load_level(current_world, next)


## True when the golden-score threshold was met. A threshold of 0 is real ("finish using
## zero portals"), so -1 is the only "unknown" sentinel.
func earned_golden_score(portals_used: int) -> bool:
	var threshold: int = DataLoader.golden_score(current_world, current_level)
	return threshold >= 0 and portals_used <= threshold
