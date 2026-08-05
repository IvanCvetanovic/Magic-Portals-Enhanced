extends Node
## Loads every JSON data file once at startup. Autoload #2 of 3 (CLAUDE.md rule 3).
##
## CLAUDE.md rule 1: all gameplay numbers live in JSON, never in a `.gd`. A designer must
## be able to retune without touching code, and the converter *generates* some of this
## data — so it has to be data in the first place.
##
## TWO SOURCES, and the split is legal, not stylistic:
##
##   res://data/*.json        OUR config. Committed. Hand-authored tuning values.
##   res://assets/data/*.json CONVERTER OUTPUT, derived from the original game. Gitignored
##                            (`.gitignore` → `**/game/assets/`), because level structure
##                            and per-level score thresholds are the original's design
##                            data in a different encoding.
##
## A fresh clone therefore has the first and not the second. That is expected: `is_ready`
## stays false until the converter has been run, and callers are told which piece is
## missing rather than reading zeros and pretending everything is fine.

const CONFIG_DIR: String = "res://data"
const CONVERTED_DIR: String = "res://assets/data"
const CHAPTERS_FILE: String = "chapters.json"

## Hand-authored config, keyed by file stem: `config["game"]["input"]["tap_max_travel_px"]`.
var config: Dictionary = {}
## Converted chapter manifest. Empty until the converter has run.
var chapters: Dictionary = {}

## True only when BOTH sources loaded. Gameplay should refuse to start otherwise —
## a half-loaded data layer produces a game that runs and is silently wrong.
var is_ready: bool = false
## Human-readable reason `is_ready` is false. Empty when ready.
var missing_reason: String = ""


func _ready() -> void:
	_load_config()
	_load_chapters()


func _load_config() -> void:
	var dir: DirAccess = DirAccess.open(CONFIG_DIR)
	if dir == null:
		_fail("no config directory at %s — the repo is incomplete" % CONFIG_DIR)
		return
	for file_name: String in dir.get_files():
		if not file_name.ends_with(".json"):
			continue
		var parsed: Variant = _read_json("%s/%s" % [CONFIG_DIR, file_name])
		if parsed == null:
			return
		config[file_name.get_basename()] = parsed


func _load_chapters() -> void:
	var path: String = "%s/%s" % [CONVERTED_DIR, CHAPTERS_FILE]
	if not FileAccess.file_exists(path):
		_fail(
			(
				"no chapter manifest at %s. Run the converter: "
				+ "python -m ethanon2godot --world 0 --copy-textures  (see README.md)"
			)
			% path
		)
		return
	var parsed: Variant = _read_json(path)
	if parsed == null:
		return
	chapters = parsed
	is_ready = true
	missing_reason = ""


func _read_json(path: String) -> Variant:
	var text: String = FileAccess.get_file_as_string(path)
	if text.is_empty():
		_fail("could not read %s" % path)
		return null
	var json: JSON = JSON.new()
	# Parse errors must be loud. A malformed data file that silently yields {} would
	# surface much later as "why is every value zero".
	if json.parse(text) != OK:
		_fail("%s is not valid JSON: line %d — %s" % [path, json.get_error_line(), json.get_error_message()])
		return null
	return json.data


func _fail(reason: String) -> void:
	is_ready = false
	missing_reason = reason
	push_warning("DataLoader: %s" % reason)


## Read a config value by path, e.g. `get_value("game", ["input", "tap_max_travel_px"], 24)`.
##
## The default is returned when the key is absent, but a warning is pushed too: a missing
## tunable means the JSON and the code have drifted apart, and silently substituting a
## literal is exactly the hardcoded-value problem rule 1 exists to prevent.
func get_value(file_stem: String, key_path: Array, default_value: Variant) -> Variant:
	var node: Variant = config.get(file_stem)
	for key: String in key_path:
		if not (node is Dictionary) or not (node as Dictionary).has(key):
			push_warning(
				"DataLoader: %s.%s missing, using default %s" % [file_stem, "/".join(key_path), default_value]
			)
			return default_value
		node = (node as Dictionary)[key]
	return node


## Number of levels in `world`, from the converted manifest. 0 if unknown.
func level_count(world: int) -> int:
	var worlds: Variant = chapters.get("worlds")
	if not (worlds is Array) or world < 0 or world >= (worlds as Array).size():
		return 0
	var entry: Variant = (worlds as Array)[world]
	if not (entry is Dictionary):
		return 0
	var levels: Variant = (entry as Dictionary).get("levels")
	return (levels as Array).size() if levels is Array else 0


## `res://` path of a level scene, or "" when out of range.
func level_scene_path(world: int, level: int) -> String:
	if level < 0 or level >= level_count(world):
		return ""
	var levels: Array = ((chapters["worlds"] as Array)[world] as Dictionary)["levels"]
	return ((levels[level]) as Dictionary).get("scene", "")


## Max portals allowed for a gold medal on this level, or -1 when unknown.
##
## A threshold of 0 is real and means "finish using zero portals" — seven levels ship
## that. Do not treat 0 as "unset"; that is why the sentinel is -1.
func golden_score(world: int, level: int) -> int:
	if level < 0 or level >= level_count(world):
		return -1
	var levels: Array = ((chapters["worlds"] as Array)[world] as Dictionary)["levels"]
	return int(((levels[level]) as Dictionary).get("golden_score", -1))
