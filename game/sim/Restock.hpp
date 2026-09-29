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
// 'barrel_spawn', 'ghost_utility_spawn.ent', 'barrel_bomb'), and the ghost's three
// (ins 594-630, the barrel as 'barrel_bomb_low_emissive.ent', on SHOOTING's beat;
// ins 632-667 on the same beat and 1047-1083 on every RAGE_MODE update, the fire
// diamond). The port plays all four (Game::AfterStep): the two barrels through this
// module (darkdragon.json and ghost.json `restock`), the fire diamond by Game's own
// lines, because a diamond is not a body.
//
// WHAT THE PORT BUILDS, AND FROM WHAT. The original makes the body from the .ent
// on disk; the port has no .ent reader in the simulation and builds only nodes. So
// the rule names the entity whose node carries the added .ent's data (its
// `template`), and the port builds that node's body afresh at the marker, unturned.
// The node is the LEVEL's own when the level places one (4-32's barrel_bomb), and
// otherwise the port's template table's (templates.json: 3-32 places no barrel at
// all, so its barrel_bomb_low_emissive.ent is built from the table). Either is a
// claim about two files, so the data states it field by field; a bare name and its
// .ent can differ (docs/ethanon-formats.md 4.5). The effect entity is not added: it
// is a picture, and not built (darkdragon.json, ghost.json).

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
    std::string templateEntity; // the port's: the entity whose node carries `add`'s data
};

// Where a level adds it. A level without the marker adds nothing, as the original
// prints and adds nothing; one without the template cannot be built by the port.
struct Site {
    bool spawnFound = false;
    glm::dvec2 spawnPx{0.0};
    // The marker's depth, its eth_z in level units (0 where it states none): AddEntity
    // is given the marker's whole vector3, so the added entity stands at this depth.
    double spawnZ = 0.0;
    std::string templateNode;
    // The template is the port's table's (templates.json), not a node of the level.
    bool templateFromPort = false;
    bool Ready() const { return spawnFound && !templateNode.empty(); }
};

// The first node of the level (in file order) whose entity is the marker, and the
// first whose entity is the template: the level's alone.
Site Resolve(const Tscn::Scene& scene, const Rule& rule);
// The same, and a template the level does not place taken from the port's table.
Site Resolve(const Tscn::Scene& scene, const Tscn::Scene& templates, const Rule& rule);

// One entity a restock added, which the level holds until it is gone.
struct Added {
    std::string name;         // the port's handle: the template's node name, '#', a count
    std::string alias;        // what SeekEntity finds it by
    std::string templateNode; // the node it was built from, whose picture it borrows
    bool templateFromPort = false; // that node is the port's table's, not the level's
    entt::entity body = entt::null;
    glm::dvec2 atPx{0.0};     // where it was added
    double z = 0.0;           // and at what depth (Site::spawnZ)
};

// The scene an added entity's template node lives in: the level's, or the table's.
const Tscn::Scene& TemplateScene(const Tscn::Scene& level, const Tscn::Scene& templates, bool fromPort);

struct State {
    std::vector<Added> added;
    int count = 0; // restocks made, which names each one
    // Restocks asked for that could not be built (Add's `why`), and the first one's
    // reason: the layer says it once, so a level that quietly goes without its
    // weapon says why (the backlog's "A refused restock is silent").
    int refusals = 0;
    std::string firstRefusal;

    // SeekEntity(search): an entity the level still holds whose name is `search`
    // exactly - one of the level's own nodes by its entity name while its body
    // stands, or one an earlier restock added by its alias. A node with no body
    // (a marker) is not counted: every caller the port wires searches for a body.
    bool Found(const Tscn::Scene& scene, const LevelBuilder::Built& built, const entt::registry& registry,
               const std::string& search) const;

    // addEntityAtSpawnPoint, the entity alone: the template's body built at the
    // marker, unturned, and entered in `built` under its new name. entt::null, with
    // `error` set and the refusal counted, when the site is not ready or the
    // template has no body.
    entt::entity Add(const Tscn::Scene& scene, const Tscn::Scene& templates, entt::registry& registry,
                     const LevelBuilder::Options& options, const Rule& rule, const Site& site,
                     LevelBuilder::Built& built, std::string& error);
};

// The port's template table (game/data/templates.json): each entity the original
// adds at run time that no level of the game places, as the converter would write
// its node, read back through Tscn::Parse - so the table is held to the same strict
// reader as a level. The scene's root carries the LEVEL's own root lighting keys
// (eth_ambient, eth_light_intensity), so the layer can read a template's look with
// the reader it reads the level's with; nothing else of the level is copied, and no
// template is built or drawn until a restock adds it. False, with `error`.
bool LoadTemplates(const std::string& path, const Tscn::Scene& level, Tscn::Scene& out, std::string& error);

} // namespace MagicPortals::Restock
