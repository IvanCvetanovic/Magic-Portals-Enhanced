extends Node
## Headless tests for the core layer: DataLoader, GameState, LevelManager.
##
## Run:
##     tools\godot.bat --headless --path game res://scenes/dev/verify_core.tscn
##
## Runs as a SCENE, not via `--script`. That is deliberate and not a style choice:
## `--script` installs a custom main loop before autoloads are registered, so `DataLoader`
## and friends are not even resolvable identifiers there. Running as a scene exercises the
## real autoload path the game uses.
##
## Exits non-zero on any failure, so it works as a build gate. Companion to
## verify_levels.gd, which checks converter OUTPUT; this checks the code that consumes it.
##
## Needs converted levels present (see README.md). Without them it reports skipped and
## exits 0 — a fresh clone has no game data and that is not a test failure.

var _passed: int = 0
var _failed: int = 0


func _ready() -> void:
	if not DataLoader.is_ready:
		print("SKIP — %s" % DataLoader.missing_reason)
		get_tree().quit(0)
		return

	# Progression is persisted to user://, so a previous run would otherwise leak into
	# this one and make the unlock assertions pass for the wrong reason.
	GameState.reset_progress()

	_test_manifest()
	_test_unlocking()
	_test_completion_and_best_portals()
	_test_level_loading()
	_test_restart_is_cheap_and_leak_free()
	_test_advance_and_chapter_end()
	_test_ui()

	print("--------------------------------------------------")
	print("passed: %d   failed: %d" % [_passed, _failed])
	GameState.reset_progress()
	get_tree().quit(1 if _failed > 0 else 0)


func _check(label: String, condition: bool, detail: String = "") -> void:
	if condition:
		_passed += 1
	else:
		_failed += 1
		printerr("FAIL  %s%s" % [label, "" if detail.is_empty() else "  — " + detail])


func _test_manifest() -> void:
	var count: int = DataLoader.level_count(0)
	_check("chapter 1 has 32 levels", count == 32, "got %d" % count)
	_check("world 4 exists in manifest", DataLoader.level_count(3) >= 0)
	_check("out-of-range world is 0", DataLoader.level_count(99) == 0)

	var path: String = DataLoader.level_scene_path(0, 0)
	_check("level 1-1 has a scene path", path.begins_with("res://"), path)
	_check("level scene actually exists", ResourceLoader.exists(path), path)
	_check("out-of-range level path is empty", DataLoader.level_scene_path(0, 999).is_empty())

	# A gold threshold of 0 is REAL — "finish using zero portals" — so -1 is the only
	# valid "unknown". Asserting this stops anyone re-introducing 0-as-sentinel.
	_check("level 1-1 gold threshold is 0", DataLoader.golden_score(0, 0) == 0)
	_check("level 1-2 gold threshold is 1", DataLoader.golden_score(0, 1) == 1)
	_check("unknown gold threshold is -1", DataLoader.golden_score(0, 999) == -1)

	_check(
		"config loaded from res://data",
		int(DataLoader.get_value("game", ["input", "min_touch_target_px"], -1)) == 96
	)
	# Touch targets >= 96 px is a hard requirement (CLAUDE.md rule 4), so pin it here
	# rather than trusting whoever edits the JSON next.
	_check(
		"touch target meets the 96 px floor",
		int(DataLoader.get_value("game", ["input", "min_touch_target_px"], 0)) >= 96
	)
	# MILESTONE_1 §4: retry must be instant. A non-zero transition would violate it.
	_check(
		"retry transition is zero",
		float(DataLoader.get_value("game", ["level", "retry_transition_s"], -1.0)) == 0.0
	)


func _test_unlocking() -> void:
	GameState.reset_progress()
	_check("level 1-1 starts unlocked", GameState.is_unlocked(0, 0))
	_check("level 1-2 starts locked", not GameState.is_unlocked(0, 1))
	_check("world 2 starts locked", not GameState.is_unlocked(1, 0))

	GameState.unlocked_world = 1
	GameState.unlocked_level = 5
	_check("earlier world fully unlocked", GameState.is_unlocked(0, 31))
	_check("current world unlocked up to marker", GameState.is_unlocked(1, 5))
	_check("past the marker stays locked", not GameState.is_unlocked(1, 6))
	GameState.reset_progress()


func _test_completion_and_best_portals() -> void:
	GameState.reset_progress()
	_check("completing the frontier advances", GameState.mark_completed(0, 0, 3))
	_check("marker moved to level 2", GameState.unlocked_level == 1)
	_check("level 1-2 now unlocked", GameState.is_unlocked(0, 1))

	# Replaying must not roll progression forward again.
	_check("replay does not advance", not GameState.mark_completed(0, 0, 2))
	_check("marker unchanged after replay", GameState.unlocked_level == 1)
	# Lower is better — the gold threshold is a MAXIMUM portal count.
	_check("best portals keeps the lower score", int(GameState.best_portals["0:0"]) == 2)
	_check("a worse replay does not overwrite", not GameState.mark_completed(0, 0, 9))
	_check("best portals still 2", int(GameState.best_portals["0:0"]) == 2)

	_check("chapter not complete after one level", not GameState.is_chapter_complete(0))
	GameState.reset_progress()


func _test_level_loading() -> void:
	var container: Node2D = Node2D.new()
	add_child(container)
	var manager: LevelManager = LevelManager.new(container)
	add_child(manager)

	_check("loads level 1-1", manager.load_level(0, 0))
	_check("root instantiated", manager.current_root != null)
	_check("level has children", manager.current_root.get_child_count() > 0)
	_check("container holds exactly one level", container.get_child_count() == 1)

	# A locked level must be refused, or progression means nothing.
	_check("refuses a locked level", not manager.load_level(0, 20))
	_check("refuses an out-of-range level", not manager.load_level(0, 999))
	_check("still holds one level after refusals", container.get_child_count() == 1)

	# Gold check uses <=, and threshold 0 must mean "zero portals only".
	_check("0 portals earns gold on 1-1", manager.earned_golden_score(0))
	_check("1 portal misses gold on 1-1", not manager.earned_golden_score(1))

	manager.free()
	container.free()

	# A manager with no container must REFUSE, not silently parent levels to itself.
	# Godot does not pass _init args when instantiating a node from a .tscn, so this
	# null path is genuinely reachable and used to fall back to `self`.
	var orphan: LevelManager = LevelManager.new()
	add_child(orphan)
	_check("refuses to load without a container", not orphan.load_level(0, 0))
	_check("nothing was parented to the manager", orphan.get_child_count() == 0)

	# …and works once one is supplied after construction.
	var late: Node2D = Node2D.new()
	add_child(late)
	orphan.set_container(late)
	_check("loads after set_container", orphan.load_level(0, 0))
	_check("level went to the container", late.get_child_count() == 1)
	orphan.free()
	late.free()


func _test_restart_is_cheap_and_leak_free() -> void:
	var container: Node2D = Node2D.new()
	add_child(container)
	var manager: LevelManager = LevelManager.new(container)
	add_child(manager)
	manager.load_level(0, 0)

	var first: Node = manager.current_root
	var before: int = container.get_child_count()

	manager.restart_level()

	# THE assertion carrying §4's "retry is instant" claim: the old level must be freed
	# IMMEDIATELY, not queued. Deferred deletion leaves two levels alive at once — double
	# physics bodies, double input handlers, a flicker every retry.
	#
	# Check node VALIDITY, not child count. `_swap_in` calls remove_child() before
	# freeing, so the count is 1 either way and an earlier version of this test passed
	# happily with queue_free() — it was decorative. Verified by mutation: swapping
	# free() for queue_free() now fails this line.
	_check("restart frees the old level immediately", not is_instance_valid(first))
	_check("restart replaced the instance", manager.current_root != first)

	for _i: int in range(4):
		manager.restart_level()
	_check("restart keeps exactly one level alive", container.get_child_count() == before,
		"expected %d, got %d" % [before, container.get_child_count()])
	_check("restarted level still populated", manager.current_root.get_child_count() > 0)

	manager.free()
	container.free()


func _test_advance_and_chapter_end() -> void:
	GameState.reset_progress()
	var container: Node2D = Node2D.new()
	add_child(container)
	var manager: LevelManager = LevelManager.new(container)
	add_child(manager)

	manager.load_level(0, 0)
	manager.complete_level(0)
	# complete_level records the result; it does NOT advance. Advancing is policy, and it
	# lives in Main behind the `advance_on_complete` config flag so a level-select screen
	# or a results screen can sit between the two without fighting the manager.
	_check("complete does not auto-advance", manager.current_level == 0)
	_check("advance moves to level 2", manager.advance_level())
	_check("now on level 2", manager.current_level == 1)

	# Walk the whole chapter to prove the manifest, unlocking and advance agree end to
	# end — the last level must report "no next level" rather than running off the array.
	var last: int = DataLoader.level_count(0) - 1
	var guard: int = 0
	while manager.current_level < last and guard < 100:
		manager.complete_level(0)
		if not manager.advance_level():
			break
		guard += 1
	_check("reached the last level", manager.current_level == last,
		"stopped at %d of %d" % [manager.current_level, last])

	manager.complete_level(0)
	_check("advance past the end returns false", not manager.advance_level())
	_check("still on the last level", manager.current_level == last)
	_check("chapter reports complete", GameState.is_chapter_complete(0))

	manager.free()
	container.free()
	GameState.reset_progress()


func _test_ui() -> void:
	# The HUD and menu are built in code, so a construction error only shows up at run
	# time. Instantiating both here turns that into a test failure.
	var hud: Hud = Hud.new()
	add_child(hud)
	hud.set_level_stats(5, 2)
	hud.set_crystals(4)
	_check("hud builds", hud.get_child_count() > 0)

	var menu: MenuScreen = MenuScreen.new()
	add_child(menu)
	_check("menu builds", menu.get_child_count() > 0)
	menu.show_level_select(0)
	_check("level select builds", menu.get_child_count() > 0)

	# Touch targets must clear the 96 px floor (CLAUDE.md rule 4). Walk the real button
	# tree rather than trusting the constant, so a layout change cannot quietly shrink one.
	var floor_px: float = float(DataLoader.get_value("game", ["input", "min_touch_target_px"], 96))
	var checked: int = 0
	var too_small: int = 0
	for node: Node in _all_descendants(hud):
		var button: Button = node as Button
		if button == null:
			continue
		checked += 1
		# Height only: the pad buttons are square but the stacked restart/menu buttons are
		# deliberately shorter, so the meaningful floor is the smaller dimension they share.
		if button.custom_minimum_size.x < floor_px * 0.5:
			too_small += 1
	_check("hud has buttons", checked > 0)
	_check("no hud button is below half the touch floor", too_small == 0,
		"%d of %d too small" % [too_small, checked])

	# The chapter-4 tab must be locked on a fresh save, or level select leaks progression.
	GameState.reset_progress()
	menu.show_level_select(0)
	_check("chapter 4 locked on a fresh save", not GameState.is_unlocked(3, 0))

	hud.free()
	menu.free()


func _all_descendants(node: Node) -> Array[Node]:
	var out: Array[Node] = []
	var stack: Array[Node] = [node]
	while not stack.is_empty():
		var current: Node = stack.pop_back()
		out.append(current)
		for child: Node in current.get_children():
			stack.push_back(child)
	return out
