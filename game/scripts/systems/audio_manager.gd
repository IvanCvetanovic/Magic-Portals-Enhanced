class_name AudioManager
extends Node
## Sound effects and music.
##
## Deliberately NOT a fourth autoload — CLAUDE.md rule 3 caps them at three (EventBus,
## GameState, DataLoader) and says not to add one without asking. This is a normal node
## under `Main`, which persists across level changes because levels are children of a
## separate container. It listens on EventBus and never needs to be called directly.
##
## Cue names map to the original's own filenames via `res://data/audio.json`, so a
## designer can re-point a cue without touching code (rule 1).
##
## Audio lives in `res://assets/soundfx/`, which is converter output and therefore
## gitignored. A fresh clone has no audio: every cue silently no-ops and the game still
## runs. That is intended — missing audio must never be fatal.

const SOUND_DIR: String = "res://assets/soundfx"

## Concurrent one-shot players. Sized so a busy moment (portal + crystal + door) never
## steals its own channels; pooled because CLAUDE.md rule 6 forbids allocating per event.
const VOICE_COUNT: int = 8

var _voices: Array[AudioStreamPlayer] = []
var _next_voice: int = 0
var _music: AudioStreamPlayer = null
var _cues: Dictionary = {}
var _cache: Dictionary = {}
var _enabled: bool = true


func _ready() -> void:
	for index: int in range(VOICE_COUNT):
		var player: AudioStreamPlayer = AudioStreamPlayer.new()
		player.name = "Voice%d" % index
		add_child(player)
		_voices.append(player)

	_music = AudioStreamPlayer.new()
	_music.name = "Music"
	add_child(_music)

	var cues: Variant = DataLoader.get_value("audio", ["cues"], {})
	_cues = cues if cues is Dictionary else {}
	_enabled = bool(DataLoader.get_value("audio", ["enabled"], true))

	EventBus.portal_placed.connect(_on_portal_placed)
	EventBus.portal_traversed.connect(_on_portal_traversed)
	EventBus.portals_consumed.connect(_on_portals_consumed)
	EventBus.level_completed.connect(_on_level_completed)
	EventBus.level_failed.connect(_on_level_failed)


## Play a named cue. Unknown or missing cues are a silent no-op, by design.
func play(cue: StringName) -> void:
	if not _enabled:
		return
	var file_name: Variant = _cues.get(String(cue))
	if file_name == null:
		return
	var stream: AudioStream = _load(String(file_name))
	if stream == null:
		return
	# Round-robin rather than "find a free player": a cue must never be dropped because
	# eight others are still ringing, and stealing the oldest is inaudible in practice.
	var player: AudioStreamPlayer = _voices[_next_voice]
	_next_voice = (_next_voice + 1) % _voices.size()
	player.stream = stream
	player.play()


func play_music(cue: StringName, looping: bool = true) -> void:
	if not _enabled:
		return
	var file_name: Variant = _cues.get(String(cue))
	if file_name == null:
		return
	var stream: AudioStream = _load(String(file_name))
	if stream == null:
		return
	if stream is AudioStreamMP3:
		(stream as AudioStreamMP3).loop = looping
	_music.stream = stream
	_music.play()


func stop_music() -> void:
	_music.stop()


func _load(file_name: String) -> AudioStream:
	if _cache.has(file_name):
		return _cache[file_name]
	var path: String = "%s/%s" % [SOUND_DIR, file_name]
	# Absence is the normal state of a fresh clone, so cache the miss too rather than
	# hitting the filesystem on every shot.
	var stream: AudioStream = null
	if ResourceLoader.exists(path):
		stream = load(path) as AudioStream
	_cache[file_name] = stream
	return stream


func _on_portal_placed(_world_position: Vector2, _active_count: int) -> void:
	play(&"portal_created")


func _on_portal_traversed(_traveller: Node2D, _entry: int, _exit: int) -> void:
	play(&"teleport")


func _on_portals_consumed(_active_count: int) -> void:
	play(&"portal_killed")


func _on_level_completed(_world: int, _level: int, _portals: int) -> void:
	play(&"level_complete")


func _on_level_failed(_world: int, _level: int, _reason: StringName) -> void:
	play(&"death")
