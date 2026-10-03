#pragma once

// 4-19's red key: red_key_state.ent, the red halo placed on the key, and the wall
// it breaks. The one placement in the game is level18c's red_key_state_ent_2754,
// on key_2701's own point; the level's only way to the rocket is past
// breakable_wall_2756, and nothing else on the level breaks it.
//
// ETHCallback_red_key_state (out/parity/asbc/all_functions.txt L97171, md5
// 48fe65ce), every frame:
//
// - #1-87, ONCE (CheckCustomData('colorSet') is empty on the first call): the
//   entity is Scale(vector2(0.8, 1.5))d, its colour set to vector3(1, 0.1, 0.1)
//   and stored as 'color', and GetAmbientLight() stored as 'ambientLight'. A call
//   pushes its last argument first (00_order 8.0 rule 3), so #14-15's 1.5, 0.8 are
//   (0.8, 1.5) and #49-51's 0.1, 0.1, 1 are (1, 0.1, 0.1). red_key_state.ent is
//   portal_halo.png, added, emissive 1 and no <Light>: its "light" is this halo's
//   colour, and the layer draws it (MagicPortalsLayer, syncLighting).
// - #224-277, until it is destroyed: seekEntityFromBucket(GetBucket(its point),
//   'key') - the FIRST entity named exactly `key` in its own bucket - and when that
//   one's GetInt('ownerID') >= 0, playRedKeyGotSound() and destroy(thisEntity),
//   which sets its 'destroy' datum (MiscCallbacks L68753).
// - #88-222, once destroyed, from the next frame: 'destroyTime' +=
//   getLastFrameElapsedTime(); bias = smoothBeginning(smoothEnd(1 - t / 6000));
//   SetColor('color' x bias) and SetAmbientLight('ambientLight' x bias); and once
//   t >= FADE_TIME 6000, DeleteEntity(thisEntity), then SeekEntity('breakable_wall')
//   - by its exact name (ETHBucketManager.cpp:394-406) - and destroy() on it, which
//   the wall's own callback reads (ETHCallback_breakable_wall L68771 #1-86): the
//   demolition sound, wall_destroy.ent, startEarthquake(20 x destroyEffectScale).
//
// So the trigger is the key TAKEN, not the key destroyed. Footage F17 (4-19, take
// r5) fits the stone row's grey level to the decoded curve with RMS 0.59 over 66
// frames, starting 0.12 s after the teleport that took the key, and shows the wall
// gone 6.15-6.25 s after that start (keys.json `red_key_state`).
//
// What the port does NOT model, recorded in the planning record (step 118):
// - the bucket-visibility clock: red_key_state is static, and a static entity's
//   callback runs only while its bucket is visible (00_order 8.12c); the port's
//   clock runs every tick, as for every static callback the port builds;
// - (the uint milliseconds are built since planning step 147: the tick hands it the
//   script's whole 16;)
// - which of two static callbacks runs first in a frame: the port takes the wall on
//   the tick the fade ends, the original on that frame or the next, by the order of
//   the visible buckets (the wall's is not the state's).

#include "sim/Keys.hpp"
#include "sim/Roles.hpp"
#include "sim/Tscn.hpp"

#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace MagicPortals::RedKey {

// The port's keys.json, its `red_key_state` block.
struct Rules {
    std::string keyEntity;  // what seekEntityFromBucket looks for, exactly: 'key'
    std::string wallEntity; // what SeekEntity looks for, exactly: 'breakable_wall'
    double fadeMs = 0.0;    // FADE_TIME
    glm::dvec3 colour{1.0}; // the halo's colour, before the fade
    glm::dvec2 scale{1.0};  // what its first call scales the entity by
    // Ethanon's default bucket, 256 of the original's screen pixels, in the level's
    // units: 256 / 2.8125 at the fixed 720 screen (R12).
    double bucketUnits = 0.0;
};

// False, with `error`, when the block or any of its numbers is missing, or a
// number is not above zero.
bool LoadRules(const std::string& path, Rules& out, std::string& error);

struct State {
    Rules rules;
    bool present = false;
    std::string name;          // its node
    glm::dvec2 atPx{0.0};      // its point, and so its bucket
    // The level's nodes whose entity is exactly rules.keyEntity: the ones a bucket
    // search by that name can find. A key a boss drops is key.ent, and is not one.
    std::vector<std::string> keyNodes;

    // Its 'destroy' datum: set on the tick the key in its bucket had an owner,
    // and the fade's clock from the next tick on.
    bool destroyed = false;
    std::string takenBy;  // the key that set it
    double elapsedMs = 0.0; // 'destroyTime'
    double bias = 1.0;      // what its colour and the ambient are multiplied by
    bool gone = false;      // deleted, at fadeMs
    std::string wallName;   // what SeekEntity found then; empty when nothing stood

    struct Turn {
        bool taken = false;       // this tick: the key had an owner (playRedKeyGotSound)
        bool destroyWall = false; // this tick: the fade ended, and the wall is to go
    };
    // One tick, after the keys have moved: `ms` of the level's clock.
    Turn Tick(const Keys::State& keys, double ms);

    // The bucket a point of the level stands in (ETHBucketManager.cpp:47-50).
    glm::ivec2 BucketOf(const glm::dvec2& atPx) const;
    // bias at `elapsedMs` of the fade: smoothBeginning(smoothEnd(1 - t / fadeMs)).
    double BiasAt(double elapsedMs) const;
};

// The level's red_key_state, when it places one. False, with `error`, for a second
// one - the game places one - or one with no position.
bool Find(const Tscn::Scene& scene, const Roles::Table& roles, const Rules& rules, State& out, std::string& error);

} // namespace MagicPortals::RedKey
