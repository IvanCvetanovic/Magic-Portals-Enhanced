#pragma once

// addEntityIfItCantBeFound: what a boss does to put the player's weapon back.
//
// THE DECODE (ETHCallback_ghost.angelscript in the listing, out/parity/asbc/
// all_functions.txt, md5 48fe65ce...; its arguments pushed last first, 00_order 8.0):
//
//   addEntityIfItCantBeFound(entityToSearch, entityToAdd, spawnEntityName,
//                            effectEntityName, alternativeName)   bytes 457261..457542
//     #1-13  SeekEntity(entityToSearch); on to the add only when it finds nothing
//     #14-27 addEntityAtSpawnPoint(entityToAdd, spawnEntityName, effectEntityName,
//            alternativeName), whose entity it returns; null when one was found
//   addEntityAtSpawnPoint                                          bytes 457542..458062
//     #1-13  SeekEntity(spawnEntityName); none, and #85-101 prints
//            'Spawn entity not found ' and adds nothing
//     #15-20 its GetPosition(), the vector3 both adds stand at
//     #24-51 AddEntity(effectEntityName, that point, angle 0, '', getScale())
//     #53-75 AddEntity(entityToAdd, that point, angle 0, alternativeName, getScale())
//
// SeekEntity compares names EXACTLY (ETHBucketManager.cpp:394-406): the both-
// spellings rule of 00_order 8.0 is the callbacks' binding, not this search. And
// an entity asked to go off but not yet gone is still there to be found.
//
// Four callers in the game: the dark dragon's, once per shot at the player
// (ETHCallback_dark_dragon ins 997-1064: 'barrel_bomb', 'barrel_bomb.ent',
// 'barrel_spawn', 'ghost_utility_spawn.ent', 'barrel_bomb'), which the port plays
// (Game::AfterStep, darkdragon.json `restock`); and the ghost's three (ins 594-630,
// the barrel as 'barrel_bomb_low_emissive.ent'; ins 632-667 and 1047-1083, the fire
// diamond), which the ghost's own step wires - its fire diamond is restocked by
// Game's own lines already, because a diamond is not a body.
//
// WHAT THE PORT BUILDS, AND FROM WHAT. The original makes the body from the .ent
// on disk; the port has no .ent reader in the simulation and builds only nodes. So
// the rule names the placed entity whose node carries the added .ent's data (its
// `template`), and the port builds that node's body afresh at the marker, unturned.
// That is a claim about two files, so the rule's note in the data states it field
// by field; a bare name and its .ent can differ (docs/ethanon-formats.md 4.5).
// The effect entity is not added: it is a picture, and not built (darkdragon.json).

#include "sim/LevelBuilder.hpp"
#include "sim/Tscn.hpp"

#include <string>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace MagicPortals::Restock {

// One caller's five arguments, and the port's template.
struct Rule {
    std::string search;         // entityToSearch, compared exactly
    std::string add;            // entityToAdd: the .ent the original adds
    std::string spawn;          // spawnEntityName: the marker both adds stand at
    std::string effect;         // effectEntityName: added first, at the same point
    std::string alias;          // alternativeName: the name the added entity answers to
    std::string templateEntity; // the port's: the placed entity whose node carries `add`'s data
};

// Where a level adds it. A level without the marker adds nothing, as the original
// prints and adds nothing; one without the template cannot be built by the port.
struct Site {
    bool spawnFound = false;
    glm::dvec2 spawnPx{0.0};
    std::string templateNode;
    bool Ready() const { return spawnFound && !templateNode.empty(); }
};

// The first node of the level (in file order) whose entity is the marker, and the
// first whose entity is the template.
Site Resolve(const Tscn::Scene& scene, const Rule& rule);

// One entity a restock added, which the level holds until it is gone.
struct Added {
    std::string name;         // the port's handle: the template's node name, '#', a count
    std::string alias;        // what SeekEntity finds it by
    std::string templateNode; // the node it was built from, whose picture it borrows
    entt::entity body = entt::null;
    glm::dvec2 atPx{0.0};     // where it was added
};

struct State {
    std::vector<Added> added;
    int count = 0; // restocks made, which names each one

    // SeekEntity(search): an entity the level still holds whose name is `search`
    // exactly - one of the level's own nodes by its entity name while its body
    // stands, or one an earlier restock added by its alias. A node with no body
    // (a marker) is not counted: every caller the port wires searches for a body.
    bool Found(const Tscn::Scene& scene, const LevelBuilder::Built& built, const entt::registry& registry,
               const std::string& search) const;

    // addEntityAtSpawnPoint, the entity alone: the template's body built at the
    // marker, unturned, and entered in `built` under its new name. entt::null, with
    // `error` set, when the site is not ready or the template has no body.
    entt::entity Add(const Tscn::Scene& scene, entt::registry& registry, const LevelBuilder::Options& options,
                     const Rule& rule, const Site& site, LevelBuilder::Built& built, std::string& error);
};

} // namespace MagicPortals::Restock
