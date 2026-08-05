extends Node
## Global signal hub. Autoload #1 of 3 (CLAUDE.md rule 3).
##
## Declarations only — EventBus holds no state and contains no logic. Anything that
## looks like a decision belongs in the system that owns it, not here. Keeping this file
## dumb is what stops it becoming a god object, which is the usual failure mode of a
## global bus.
##
## Emit through here only for genuinely cross-cutting events. Two nodes that already know
## about each other should just connect directly; routing them through a global bus makes
## the flow harder to follow, not easier.

## --- Input intents ------------------------------------------------------------
## Emitted by InputController ONLY. Gameplay never reads InputEvent directly
## (CLAUDE.md rule 4), so these are the single entry point for player intent, and the
## touch and mouse paths converge before they get here.

## A discrete press at a world position. `intent` is an InputController.Intent value.
signal intent_pressed(intent: int, world_position: Vector2)
## Release of a previously pressed intent. `duration_s` lets a system tell tap from hold
## without tracking press time itself.
signal intent_released(intent: int, world_position: Vector2, duration_s: float)
## Continuous directional input, already normalised. Zero vector means "no input".
signal move_intent_changed(direction: Vector2)

## --- Level lifecycle ----------------------------------------------------------
## Owned by LevelManager. `world` and `level` are always 0-based to match the original's
## on-disk indexing; only the UI adds 1 (docs/ethanon-formats.md, 2026-08-04 block).

signal level_load_requested(world: int, level: int)
signal level_loaded(world: int, level: int)
## `portals_used` feeds the golden-score check — the original awards gold for finishing
## at or under a per-level portal threshold.
signal level_completed(world: int, level: int, portals_used: int)
signal level_failed(world: int, level: int, reason: StringName)
signal level_restarted(world: int, level: int)
signal chapter_completed(world: int)

## --- Portals ------------------------------------------------------------------
## Owned by PortalSystem.

## A traveller went through a portal. `entry_index`/`exit_index` are the original's own
## portal indices (from the level data's `index`/`destiny` custom fields); -1 for a
## player-placed portal, which has no index in the level data.
signal portal_traversed(traveller: Node2D, entry_index: int, exit_index: int)

## The player placed a portal. `active_count` is how many placed portals now exist.
signal portal_placed(world_position: Vector2, active_count: int)

## Placed portals were destroyed by being travelled through — they are single-use.
signal portals_consumed(active_count: int)

## --- Entity behaviours -----------------------------------------------------------

## A numbered switch channel changed state. Buttons emit, switched doors listen. The
## channel is `idx` on the button and `switchIdx` on the door — same number = linked.
signal channel_changed(channel: int, pressed: bool)

## Something was mirrored by a reflect_agent.
signal reflected(body: Node2D)

## An entity was removed by the destruction economy. `cause` is one of `demolished`,
## `exploded` or `burned`.
signal entity_destroyed(entity: Node2D, cause: StringName)

## A burnable entity caught fire. It is removed when its burn timer expires.
signal entity_ignited(entity: Node2D)

## A turret fired.
signal projectile_fired(projectile: Node2D)

## An entity_launcher wants to spawn `entity_name`. Unhandled for now: the converter emits
## levels, not per-entity scenes, so there is nothing to instance by name yet. Announced
## rather than silently dropped.
signal spawn_requested(entity_name: String, origin: Vector2, bounds: Rect2)

## --- Progression --------------------------------------------------------------
## Owned by GameState.

## Fired when the furthest-unlocked marker actually advances, not on every level finish.
signal progress_unlocked(world: int, level: int)
