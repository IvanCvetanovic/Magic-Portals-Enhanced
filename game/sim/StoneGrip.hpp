#pragma once

// THE GRIP BETWEEN THE PLAYER AND A ROLLING STONE (planning step 216; game/data/stone_grip.json holds the numbers and
// where they come from).
//
// The original builds the player from dark_mage.ent (friction 0.9 on both circles of its compound) and a stone from
// rolling_stone.ent (friction 0.025, all 27 placements the same), and Box2D mixes a pair's friction as
// sqrt(friction A x friction B) when the contact is made: 0.150. Nothing in the script ever changes a friction
// (the listing has no SetFriction call). That friction is what the player stands on a stone by: it acts on the player
// (whose velocity the script then overwrites, so it barely moves him) and, as the equal and opposite impulse, on the STONE,
// whose roll and speed it brakes while someone stands on it.
//
// The port builds the player with friction 0 (Player::kFriction: his walk is set outright every tick and a gripping body
// would fight it, and the engine integrates before it solves, so a friction on him cannot hold him) and every body with the
// held friction 1, so the pair mixed to 0: the stone ran out from under a standing player and rolled across the level.
// The engine mixes the same way (sqrt, read per contact per step from each body's own `rigid.friction`), and a friction
// is per BODY (the player has one value for everything he touches), so: on the ticks a stone is within reach of the
// player, his friction is set to what makes the engine's mix of him and THAT stone come to the decoded 0.15 - mix^2 /
// the stone's own friction, 0.0225 against a stone at the held 1 - and put back as it was on the first tick without one.
// SO ON THOSE TICKS EVERY CONTACT HE HAS mixes with the raised value, not the stone's alone: his floor, a wall or a crate
// against the held 1 mixes at sqrt(0.0225 x 1) = 0.15 where it was 0 (the original's own is 0.9 against his floors,
// 0.6 against block00: the port stays below it, as it always has). The engine integrates before it solves and Steer sets
// his x outright, so that costs his walk nothing on the ground; what it can do is drag him a little vertically against a
// wall beside a stone, and push on a crate he pushes while a stone is near (inference, not measured). A tick with no
// stone in reach is bit-identical to the build before this step.
//
// NOT BUILT: the original's friction also acts on the player's own position inside the step it is applied in (at most
// 0.167 units a step, and the script's SetLinearVelocity erases it the next frame); the engine integrates a body
// before it solves, so the port's player feels it only as a velocity Steer overwrites. What the port gets is the half
// that lasts: the reaction on the stone. The stone's own friction against its floor stays the held 1, against the
// original's 0.07 (a platform) to 0.11 (1-09's bridge01) (materials.json decodes each floor's friction); the stone's
// restitution on 1-09 is 0.3 in the original's scene and 0.15 in the table. The hold cone and every other figure of the
// original's side are a Box2D MODEL (stone_grip.json says so): nothing of the original was measured with a player on a stone.

#include <string>
#include <vector>

#include <entt/entt.hpp>

namespace MagicPortals::StoneGrip {

// game/data/stone_grip.json.
struct Rules {
    // dark_mage.ent's friction and rolling_stone.ent's: the two fixtures' own numbers, whose Box2D mix is the pair's.
    double playerFriction = 0.0;
    double stoneFriction = 0.0;
    // How far past touching a stone still counts as one the player is on, in pixels: the port's own safety margin (a
    // _guess, see the json; a circle against a circle has no skin in Box2D, earthquake.json impacts.circle_touch_px 0).
    // The distance the two could close in this step is added on top, so the first step of a landing already has it.
    double touchPx = 0.0;

    // sqrt(player x stone): 0.150 for the decoded numbers.
    double Mix() const;
};

bool LoadRules(const std::string& path, Rules& out, std::string& error);

// What a level keeps of the grip between ticks: the friction the player had before it was put on, so it can be put back.
struct State {
    bool on = false;
    float savedFriction = 0.0f;
    // Ticks the grip was on in this level: a count for the suites, never read by play.
    int ticks = 0;
};

// Before a physics step, after the player has been steered: puts the grip on or takes it off. `stones` are the bodies of
// the level's stones (Demolish::State::stones: placed, thrown and the boss's rocks). `physicsS` is the step the engine is
// about to take. Returns whether the grip is on for this step.
bool Apply(entt::registry& registry, entt::entity player, const Rules& rules, const std::vector<entt::entity>& stones,
           double physicsS, State& state);

} // namespace MagicPortals::StoneGrip
