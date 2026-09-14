extends Node
## Progression and save state. Autoload #3 of 3 (CLAUDE.md rule 3).
##
## PROGRESSION MODEL — measured, not assumed. The original stores exactly two integers,
## `lastWorldAllowed` and `lastLevelAllowed`: a single high-water mark, NOT a per-level
## star tally. Levels are not gated on performance — a skip-level button exists, and
## medals are cosmetic (`MILESTONE_1.md` §7.3, resolved 2026-08-04; see the correction
## block in `docs/ethanon-formats.md`).
##
## The original's second gate — free-versus-paid purchase — is deliberately NOT modelled.
## It is a store concern, out of scope for this milestone, and inventing it now would bake
## in a monetisation decision nobody has made.

const SAVE_PATH: String = "user://progress.json"
const SAVE_VERSION: int = 1

## Furthest level the player has unlocked. Both 0-based, matching the original's on-disk
## indexing; only UI adds 1 ("1-9" is world 0, level 8).
var unlocked_world: int = 0
var unlocked_level: int = 0

## Where the player currently is. Not the same as the unlock marker — replaying an
## earlier level must not roll progression backwards.
var current_world: int = 0
var current_level: int = 0

## Best portal count per level, keyed "world:level". Feeds the golden-score medal.
## Absent key means "never completed", which is different from "completed with 0 portals".
var best_portals: Dictionary = {}

## False while a dev tool drives the game with injected progression (parity capture), so a
## level that happens to finish mid-capture cannot write that fake unlock into the real save.
var persist_enabled: bool = true


func _ready() -> void:
	load_progress()


## True when `level` of `world` may be entered.
##
## Strictly ordered on (world, level): a later world is unlocked regardless of level, and
## within the unlocked world everything up to and including the marker is open.
func is_unlocked(world: int, level: int) -> bool:
	if world < unlocked_world:
		return true
	if world > unlocked_world:
		return false
	return level <= unlocked_level


## Record a completed level and advance the marker if this was the frontier.
##
## Returns true when progression actually moved. Replaying a finished level returns
## false — it can still improve `best_portals`, but it must not re-trigger unlock effects.
func mark_completed(world: int, level: int, portals_used: int) -> bool:
	var key: String = "%d:%d" % [world, level]
	# Lower is better: the golden score is a MAXIMUM portal count.
	if not best_portals.has(key) or portals_used < int(best_portals[key]):
		best_portals[key] = portals_used

	var advanced: bool = false
	var level_total: int = DataLoader.level_count(world)
	if world == unlocked_world and level == unlocked_level:
		if level + 1 < level_total:
			unlocked_level = level + 1
			advanced = true
		elif level_total > 0:
			# Chapter cleared. Only chapter 1 is in scope for this milestone, so the
			# next world is not opened here — LevelManager emits chapter_completed and
			# whoever owns the meta layer decides what happens next.
			advanced = false

	save_progress()
	if advanced:
		EventBus.progress_unlocked.emit(unlocked_world, unlocked_level)
	return advanced


## True when every level of `world` has a recorded completion.
func is_chapter_complete(world: int) -> bool:
	var total: int = DataLoader.level_count(world)
	if total <= 0:
		return false
	for level: int in range(total):
		if not best_portals.has("%d:%d" % [world, level]):
			return false
	return true


func save_progress() -> void:
	if not persist_enabled:
		return
	var file: FileAccess = FileAccess.open(SAVE_PATH, FileAccess.WRITE)
	if file == null:
		push_warning("GameState: could not write %s" % SAVE_PATH)
		return
	file.store_string(
		JSON.stringify(
			{
				"version": SAVE_VERSION,
				"unlocked_world": unlocked_world,
				"unlocked_level": unlocked_level,
				"best_portals": best_portals,
			}
		)
	)


func load_progress() -> void:
	if not FileAccess.file_exists(SAVE_PATH):
		return
	var json: JSON = JSON.new()
	if json.parse(FileAccess.get_file_as_string(SAVE_PATH)) != OK:
		# A corrupt save must not wipe progress silently. Leave defaults, keep the file,
		# and say so — the player may prefer to recover it than to start over.
		push_warning("GameState: %s is corrupt; starting fresh but NOT overwriting it" % SAVE_PATH)
		return
	var data: Variant = json.data
	if not (data is Dictionary):
		return
	var save: Dictionary = data
	if int(save.get("version", 0)) != SAVE_VERSION:
		push_warning("GameState: save version %s != %d, ignoring" % [save.get("version"), SAVE_VERSION])
		return
	unlocked_world = int(save.get("unlocked_world", 0))
	unlocked_level = int(save.get("unlocked_level", 0))
	var best: Variant = save.get("best_portals", {})
	best_portals = best if best is Dictionary else {}


## Wipe progression. Used by tests and any future "reset progress" UI.
func reset_progress() -> void:
	unlocked_world = 0
	unlocked_level = 0
	best_portals = {}
	current_world = 0
	current_level = 0
	save_progress()
