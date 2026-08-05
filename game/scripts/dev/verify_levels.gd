extends SceneTree
## Headless smoke test for converter output — MILESTONE_1.md §7.7.
##
## Loads every converted level and reports what actually instantiated. This is the
## go/no-go for the whole pipeline: a `.tscn` that Godot's importer accepts can still
## fail to instantiate (missing texture, bad SubResource id, malformed node path), and
## `--import` alone does not catch that.
##
## Run:
##     godot --headless --path game --script res://scripts/dev/verify_levels.gd
##
## Exits non-zero if any level fails, so it is usable as a build gate.
##
## Levels live under `res://assets/levels/` because converted level data is
## Asantee-origin data in another encoding, so it sits behind the same gitignore gate
## as converted art (see `.gitignore` → `**/game/assets/`). A fresh clone therefore has
## no levels and this script reports zero found — that is correct, not a failure.

const LEVELS_DIR: String = "res://assets/levels"


func _initialize() -> void:
	var dir: DirAccess = DirAccess.open(LEVELS_DIR)
	if dir == null:
		print("no levels at %s — run the converter first (tools/converter)" % LEVELS_DIR)
		quit(0)
		return

	var names: PackedStringArray = dir.get_files()
	names.sort()

	var checked: int = 0
	var failed: int = 0
	var total_nodes: int = 0
	var total_sprites: int = 0
	var total_colliders: int = 0

	for file_name: String in names:
		if not file_name.ends_with(".tscn"):
			continue
		checked += 1
		var path: String = "%s/%s" % [LEVELS_DIR, file_name]

		var packed: PackedScene = load(path) as PackedScene
		if packed == null:
			printerr("FAIL  %s — did not load as PackedScene" % file_name)
			failed += 1
			continue

		var root: Node = packed.instantiate()
		if root == null:
			printerr("FAIL  %s — loaded but would not instantiate" % file_name)
			failed += 1
			continue

		var counts: Dictionary = _count(root)
		total_nodes += int(counts["nodes"])
		total_sprites += int(counts["sprites"])
		total_colliders += int(counts["colliders"])

		# A level that instantiates but is empty is a silent conversion failure, and
		# the one an eyeball would miss — treat it as an error, not a curiosity.
		if int(counts["nodes"]) <= 1:
			printerr("FAIL  %s — instantiated with no child nodes" % file_name)
			failed += 1
		root.free()

	print("--------------------------------------------------")
	print("levels checked : %d" % checked)
	print("failed         : %d" % failed)
	print("nodes total    : %d" % total_nodes)
	print("sprites total  : %d" % total_sprites)
	print("colliders total: %d" % total_colliders)
	quit(1 if failed > 0 else 0)


func _count(node: Node) -> Dictionary:
	var nodes: int = 0
	var sprites: int = 0
	var colliders: int = 0
	var stack: Array[Node] = [node]
	while not stack.is_empty():
		var current: Node = stack.pop_back()
		nodes += 1
		if current is Sprite2D:
			sprites += 1
		elif current is CollisionShape2D or current is CollisionPolygon2D:
			colliders += 1
		for child: Node in current.get_children():
			stack.push_back(child)
	return {"nodes": nodes, "sprites": sprites, "colliders": colliders}
