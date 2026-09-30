#include "sim/Game.hpp"

#include "core/Components.hpp"
#include "core/PhysicsSettings.hpp"
#include "core/PhysicsSystem.hpp"
#include "sim/Shot.hpp"
#include "sim/Trigger.hpp"
#include "sim/Units.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <system_error>
#include <utility>

namespace MagicPortals::Game {

namespace {

// A body a launcher threw and has taken back, dropped from what tracked it.
void Forget(Level& level, entt::entity body) {
    auto& travellers = level.portals.travellers;
    travellers.erase(std::remove(travellers.begin(), travellers.end(), body), travellers.end());
    auto& stones = level.demolish.stones;
    stones.erase(std::remove_if(stones.begin(), stones.end(),
                                [body](const Demolish::Stone& stone) { return stone.body == body; }),
                 stones.end());
}

// What a restock added, taken on as Fire::Find and Portals::Find take a placed
// body: a bomb or a burnable by its node's flags, and a traveller when its node
// says teleportable. By the TEMPLATE's node, which is the entity it was built from:
// the level's own, or the port's table's.
void TakeOnAdded(const Data& data, Level& level, const Restock::Added& added) {
    const Tscn::Node* node =
        Restock::TemplateScene(data.scene, data.templates, added.templateFromPort).FindNode(added.templateNode);
    if (node == nullptr) return;
    level.fire.Adopt(*node, added.name, added.body);
    double teleportable = 0.0;
    const Tscn::Value* flag = node->Meta("teleportable");
    if (flag != nullptr && flag->AsNumber(teleportable) && teleportable != 0.0) {
        level.portals.travellers.push_back(added.body);
    }
}

// The beholder dead: the level's button goes to its button_dest, its box and
// its body both, so what presses it and what draws it follow. The script then
// calls computeButtonInterpolator again (ETHCallback_beholder #1315), which
// rebases both of the button's stored points and leaves it at rest there -
// without it, the next frame would put the button back at its old pressedPos.
//
// NOT reproduced (D10_check section 4, which says not to): the original's rebase
// zeroes numTouchingEntities without clearing `active`, so a body still touching
// the button at that moment can leave the count at -1 and the door open forever.
void RaiseButton(entt::registry& registry, Level& level) {
    const glm::vec3 at = Units::ToWorld(level.boss.buttonDestPx.x, level.boss.buttonDestPx.y);
    for (Puzzle::Button& button : level.channels.buttons) {
        if (button.name == level.boss.buttonNode) button.Rebase(registry, glm::vec2(at.x, at.y));
    }
}

} // namespace

bool LoadData(const std::string& levelPath, const std::string& dataDirectory,
              const std::filesystem::path& prismDirectory, Data& out, std::string& error,
              const std::string& portDataDirectory) {
    Data read;
    if (!Tscn::Load(levelPath, read.scene, error)) return false;
    if (!Roles::Load(dataDirectory + "/entity_roles.json", read.roles, error)) return false;
    if (!Player::LoadTuning(dataDirectory + "/player.json", read.tuning, error)) return false;
    if (!Goals::LoadRules(dataDirectory + "/portals.json", read.goals, error)) return false;
    if (!Portals::LoadRules(dataDirectory + "/portals.json", read.portals, error)) return false;
    // Enhanced's portals.json marks its transit block a guess. The port plays
    // the original's rule, decoded, from its own transit.json.
    if (!Portal::LoadTransit(portDataDirectory + "/transit.json", read.portals.transit, error)) return false;
    // And over its guessed collision_radius_px, the two decoded radii: a
    // portal's own, and the quite separate one an antiportal refuses a tap in.
    if (!Portals::LoadPlacement(portDataDirectory + "/placement.json", read.portals, error)) return false;
    // linearMotion's shared arithmetic - the frame its step is capped at and the
    // turn it wraps at - which a placed portal's own hover is taken over
    // (ETHCallback_portal #1-10). One reading of motions.json rather than a second
    // copy of two decoded constants; the layer keeps its own for the pictures it
    // moves, and both read the same file.
    if (!Motion::LoadRules(portDataDirectory + "/motions.json", read.motion, error)) return false;
    read.portals.motion = read.motion;
    if (!Mover::LoadRules(portDataDirectory + "/movers.json", read.movers, error)) return false;
    // The same file, its `button` block: a button's sink is not a mover's swing,
    // and its numbers are decoded where the other two are the Godot project's guesses.
    if (!Puzzle::LoadRules(portDataDirectory + "/movers.json", read.puzzle, error)) return false;
    if (!Demolish::LoadRules(portDataDirectory + "/demolish.json", read.demolish, error)) return false;
    if (!Launchers::LoadRules(portDataDirectory + "/launchers.json", read.launchers, error)) return false;
    if (!Shot::LoadRules(portDataDirectory + "/shot.json", read.shot, error)) return false;
    if (!Boss::LoadRules(portDataDirectory + "/boss.json", read.boss, error)) return false;
    if (!Turrets::LoadRules(portDataDirectory + "/turrets.json", read.turrets, error)) return false;
    if (!Fire::LoadRules(portDataDirectory + "/fire.json", read.fire, error)) return false;
    if (!Hinge::LoadRules(portDataDirectory + "/hinge.json", read.hinge, error)) return false;
    if (!Minions::LoadRules(portDataDirectory + "/minions.json", read.minions, error)) return false;
    if (!Keys::LoadRules(portDataDirectory + "/keys.json", read.keys, error)) return false;
    if (!RedKey::LoadRules(portDataDirectory + "/keys.json", read.redKey, error)) return false;
    if (!Diamonds::LoadRules(portDataDirectory + "/diamonds.json", read.diamonds, error)) return false;
    if (!Fields::LoadRules(portDataDirectory + "/fields.json", read.fields, error)) return false;
    if (!Ghost::LoadRules(portDataDirectory + "/ghost.json", read.ghost, error)) return false;
    if (!Dragon::LoadRules(portDataDirectory + "/dragon.json", read.dragon, error)) return false;
    if (!DarkDragon::LoadRules(portDataDirectory + "/darkdragon.json", read.darkDragon, error)) return false;
    if (!Torch::LoadRules(portDataDirectory + "/torch.json", read.torch, error)) return false;
    // Read here, beside torch.json, although only the layer draws with it: the
    // ambient a level is drawn with follows its torch, and a suite of the
    // simulation's has to be able to ask for it without a layer.
    if (!Lighting::LoadRules(portDataDirectory + "/lighting.json", read.lighting, error)) return false;
    if (!Zerog::LoadRules(portDataDirectory + "/zerog.json", read.zerog, error)) return false;
    if (!Bounce::LoadRules(portDataDirectory + "/bounce.json", read.bounce, error)) return false;
    if (!GravityWell::LoadRules(portDataDirectory + "/gravitywell.json", read.wells, error)) return false;
    // And what Enhanced's role table calls a hazard but the original does not.
    if (!Hazards::LoadRules(portDataDirectory + "/hazards.json", read.hazards, error)) return false;
    // The entities a boss adds that no level places (sim/Restock): read beside the
    // level, whose root lighting the table's scene takes, and built by no one until
    // a restock asks for one.
    if (!Restock::LoadTemplates(portDataDirectory + "/templates.json", read.scene, read.templates, error)) return false;
    std::error_code ec;
    std::filesystem::create_directories(prismDirectory, ec);
    read.prisms = prismDirectory;
    out = std::move(read);
    return true;
}

void UseRemakeGravity(entt::registry& registry) {
    Supersonic::PhysicsSettings settings;
    settings.gravity = glm::vec3(0.0f, -Units::ToMetres(Units::kRemakeWorldGravityPx), 0.0f);
    registry.ctx().insert_or_assign<Supersonic::PhysicsSettings>(std::move(settings));
}

void UseNoGravity(entt::registry& registry) {
    Supersonic::PhysicsSettings settings;
    settings.gravity = glm::vec3(0.0f);
    registry.ctx().insert_or_assign<Supersonic::PhysicsSettings>(std::move(settings));
}

bool Start(const Data& data, entt::registry& registry, Level& out, std::string& error, bool withStatics) {
    // The two level-property flags, and they are no longer the same case.
    //
    // no_gravity is now PLAYED. It is a movement mode rather than a switch, and
    // that is exactly why it was refused until all three of its parts existed:
    // the world's gravity goes to zero here, Player::Steer is not run at all
    // (BeforeStep says why that one absence is both the fall and the walk), and
    // Portals::Shoot shoves the player away from each tap it takes. Started with
    // any one of those missing, a zero-gravity level would look like a level that
    // works and be a room with no way out. Zerog.hpp holds the decode.
    //
    // darkest is CARRIED instead. Its whole effect in the original's own
    // level-properties reader is one SetAmbientLight(DARKEST_AMBIENT_LIGHT), which
    // is drawing, not play: the layer draws the level with that ambient
    // (Lighting::Ambient) and nothing here changes. Refusing the level did not
    // make that more honest - it only hid twelve levels whose every ROLE the port
    // already plays or does not. Its one gameplay consequence is that a minion in
    // such a level is blind, and minion sight is unbuilt in every level alike;
    // minions.json says so.
    bool darkest = false;
    bool noGravity = false;
    for (const Tscn::Node& node : data.scene.nodes) {
        if (node.parent != "." || Roles::RoleOf(data.roles, node) != Roles::kLevelProperties) continue;
        double on = 0.0;
        if (const Tscn::Value* value = node.Meta(data.zerog.flagName.c_str());
            value != nullptr && value->AsNumber(on) && on != 0.0) {
            noGravity = true;
        }
        on = 0.0;
        if (const Tscn::Value* value = node.Meta("darkest"); value != nullptr && value->AsNumber(on) && on != 0.0) {
            darkest = true;
        }
        // `shadowless_door` is read by the LAYER and not here (buildSprites,
        // art.json `door`): its whole effect is the exit door's sheet and the glow
        // behind it, and nothing in the simulation acts on it.
    }

    if (noGravity) {
        UseNoGravity(registry);
    } else {
        UseRemakeGravity(registry);
    }
    LevelBuilder::Options options;
    options.prismDirectory = data.prisms;
    options.roles = &data.roles;
    options.withStatics = withStatics;
    out = Level{};
    out.darkest = darkest;
    out.noGravity = noGravity;
    if (!LevelBuilder::Build(data.scene, registry, options, out.built, error)) return false;
    if (withStatics && !Puzzle::Wire(data.scene, data.roles, out.built, data.puzzle, out.channels, error)) {
        return false;
    }
    if (withStatics && !Mover::Wire(data.scene, data.roles, out.built, data.movers, out.movers, error)) return false;
    // Guarded by withStatics for the reason Mover::Wire is: a bouncer's body is
    // a static node, so with the statics left out there is nothing to wire to
    // and every one of them would be reported as not built.
    if (withStatics && !Bounce::Wire(data.scene, data.roles, out.built, data.bounce, out.bounce, error)) return false;

    const Tscn::Node* spawn = nullptr;
    for (const Tscn::Node& node : data.scene.nodes) {
        if (node.parent == "." && Roles::RoleOf(data.roles, node) == Roles::kPlayerSpawn) {
            spawn = &node;
            break;
        }
    }
    const Tscn::Value* at = spawn != nullptr ? spawn->Find("position") : nullptr;
    if (at == nullptr || at->kind != Tscn::Value::Kind::Vector2) {
        error = "the level has no player_spawn with a position";
        return false;
    }
    out.player = Player::Spawn(registry, glm::dvec2(at->numbers[0], at->numbers[1]), data.tuning);

    if (!Goals::Find(data.scene, data.roles, data.goals, out.goals, error)) return false;
    if (!Hazards::Find(data.scene, data.roles, data.hazards, out.hazards, error)) return false;
    if (!Demolish::Find(data.scene, data.roles, out.built, data.demolish, out.demolish, error)) return false;
    if (!Launchers::Find(data.scene, data.roles, data.launchers, out.launchers, error)) return false;
    if (!Boss::Find(data.scene, data.roles, data.boss, data.launchers, out.boss, error)) return false;
    if (!Turrets::Find(data.scene, data.roles, out.built, data.turrets, out.turrets, error)) return false;
    if (!Fire::Find(data.scene, data.roles, out.built, data.fire, out.fire, error)) return false;
    if (!Hinge::Find(data.scene, data.roles, out.built, registry, data.hinge, out.hinge, error)) return false;
    if (!Minions::Find(data.scene, data.roles, data.minions, out.minions, error)) return false;
    if (!Keys::Find(data.scene, data.roles, out.built, data.keys, out.keys, error)) return false;
    if (!RedKey::Find(data.scene, data.roles, data.redKey, out.redKey, error)) return false;
    if (!Diamonds::Find(data.scene, data.roles, data.diamonds, out.diamonds, error)) return false;
    if (!Fields::Find(data.scene, data.roles, out.built, data.fields, out.fields, error)) return false;
    if (!Ghost::Find(data.scene, data.roles, data.ghost, out.ghost, error)) return false;
    // Where its barrel is put back, and from what: level31b places no barrel, so the
    // port's table (Restock::Resolve takes the level's own first).
    if (out.ghost.present) out.ghost.restockSite = Restock::Resolve(data.scene, data.templates, data.ghost.restock);
    if (!Torch::Find(data.scene, data.roles, data.torch, out.torch, error)) return false;
    if (!Portals::Find(data.scene, data.roles, out.built, registry, out.player, data.portals, data.movers, data.shot,
                       out.portals, error)) {
        return false;
    }

    // And what a tap does to the player who fired it, which in these levels is
    // the whole of how they move. Set here rather than found by Portals, because
    // it is the LEVEL's property and not anything the level places.
    out.portals.noGravity = noGravity;
    out.portals.recoilMps = data.zerog.recoilMetresPerSecond;

    // The gravity wells. A WELL REFUSES NO PORTAL AND STOPS NO SHOT (00_order R2,
    // decided for the original; the remaster doc's step 125). The antiportal.ent
    // its agent adds on its first tick is renamed gravity_area as it is added
    // (ETHScene.cpp:264-266), after AntiPortalManager's constructor has gathered
    // its fields by name (104925..105343 #16-35), so no tap is ever tested
    // against it: it is the ring and the force, and no zone. And the shot flies
    // through the agent's own body: projectile.ent has no shape, and the ray
    // that judges a tap passes gravity_agent by name (placement.json
    // final_pos.ray_ignores). The agent stays solid to everything else.
    if (!GravityWell::Find(data.scene, data.roles, data.wells, out.wells, error)) return false;
    out.wells.noGravity = noGravity;
    for (const GravityWell::Well& well : out.wells.wells) {
        if (const auto body = out.built.entities.find(well.name); body != out.built.entities.end()) {
            out.portals.shotPasses.push_back(body->second);
        }
    }

    // Chapter 2's boss. It needs the BUILT level, not just the scene: the claw's
    // whitelist is four scenery names, and what it takes away are those bodies.
    if (!Dragon::Find(data.scene, data.roles, out.built, data.dragon, out.dragon, error)) return false;

    // Chapter 4's boss. It places no body and reads five markers by name, so it
    // needs the scene and nothing the builder made.
    if (!DarkDragon::Find(data.scene, data.roles, data.darkDragon, out.darkDragon, error)) return false;

    // And the minions the markers ask for, built last: a minion is teleportable,
    // so it joins the portals' travellers the way what a launcher throws does -
    // after Portals::Find, which is what fills that list to begin with.
    for (const entt::entity body : out.minions.Spawn(registry)) out.portals.travellers.push_back(body);
    return true;
}

void BeforeStep(const Data& data, entt::registry& registry, Level& level, float direction, float dt) {
    level.channels.Tick(registry, dt);
    level.movers.Tick(registry, dt);
    // The bobbing slabs, beside the platforms and for the same reason: what sets
    // a body's position belongs before the step, what judges one after it.
    level.bounce.Tick(registry, dt);
    // And the wells, which add to a velocity rather than set a position - so
    // they belong here too, before the solver reads what they wrote, and after
    // the movers so a body riding a platform is not pulled off it mid-tick.
    level.wells.Tick(registry, dt);
    // Chapter 2's boss, and it belongs on this side for all three of the reasons
    // this half exists: it sets its own position, it spits as Turrets::Fire does,
    // and it takes geometry away BEFORE the solver reads it, so no contact is
    // ever resolved against a platform that is about to vanish.
    if (level.dragon.present) {
        const Dragon::State::Turn turn = level.dragon.Tick(registry, level.player, level.goals.completed, dt);
        // The claw's four names are scenery - none is in entity_roles.json - so
        // no module walks a list of them behind this. The removal still goes
        // through Forget, as a burnt crate does: a crushed platform cannot be a
        // traveller or a stone, and the funnel is the port's one way out.
        for (const entt::entity gone : turn.crushed) {
            if (registry.valid(gone)) registry.destroy(gone);
            Forget(level, gone);
        }
        for (const std::string& gone : turn.crushedNames) level.built.entities.erase(gone);
        if (turn.fired) {
            // addFireball is the carranca's own function, so this is the same
            // fireball.ent the fire diamond's conversion makes - only this one
            // carries killMainCharacter set.
            Turrets::Fireball made;
            made.name = level.dragon.name + "#" + std::to_string(level.dragon.fired);
            made.atPx = turn.firePx;
            made.velocityPx = turn.aimPx * level.turrets.rules.speedPx;
            made.killsPlayer = true;
            level.turrets.fireballs.push_back(made);
        }
    }
    // What a launcher throws travels and breaks walls as a stone the level
    // places does.
    for (const Launchers::Thrown& thrown : level.launchers.Throw(registry, dt)) {
        if (thrown.is.teleportable) level.portals.travellers.push_back(thrown.body);
        if (thrown.is.demolisher) level.demolish.stones.push_back(Demolish::Stone{thrown.name, thrown.body});
    }
    // And what the carrancas spit. A fireball is a point and a radius with no
    // entity, so nothing here hands it to the portals as a thrown stone is
    // handed; Turrets.hpp says why transit is a step of its own.
    level.turrets.Fire(dt);
    // The minions walk before the step, as the player is steered before it: what
    // sets a velocity belongs on this side, what judges a position on the other.
    level.minions.Tick(registry, dt);
    level.boss.BeforeStep(registry);
    // In a zero-gravity level the player is NOT STEERED AT ALL, and that single
    // absence is both halves of the original's behaviour at once:
    // MainCharacter::update skips ScreenPad::update entirely, so the two walking
    // buttons do nothing; and Steer applies the fall every tick (Player.hpp says
    // why it must, for a dynamic body with no floor snap), which is a fall this
    // level's V2_ZERO gravity does not have.
    //
    // Skipping the CALL rather than branching inside Steer leaves the most
    // heavily pinned function in the port untouched - test_mp_play walks
    // level30's seam through it at friction 0 and 1 and prints both.
    if (level.player != entt::null && !level.noGravity) {
        Player::Steer(registry, level.player, data.tuning, direction, dt);
    }
}

void AfterStep(const Data& data, entt::registry& registry, Level& level, float dt) {
    level.goals.Tick(registry, level.player, dt);
    level.hazards.Tick(registry, level.player);
    // The fireballs fly on where the step left the player, and judge it there,
    // as the hazards above them do.
    level.turrets.Tick(registry, level.player, dt);
    level.demolish.Tick(registry);
    // The fire agents, what is burning down, and the bombs - before the portals,
    // which move things. What it takes away goes through Forget as a thrown body
    // does: a crate is teleportable, so a burning one can be in portals.travellers.
    for (const entt::entity gone : level.fire.Tick(registry, level.player, dt)) Forget(level, gone);
    // What each blast grabbed with a clear line has its `destroy` set (explode,
    // listing L67590, #111-122), and a breakable wall's own callback reads that and
    // deletes it (ETHCallback_breakable_wall, L68771, #1-86) ON THE SAME FRAME: the
    // barrel is a dynamic entity and the wall a static one, and Ethanon runs the
    // static entities' callbacks after the dynamic ones (ETHScene.cpp:464-491).
    // Demolish owns the breaking, and says which breakables a blast breaks.
    for (const Fire::Grab& grab : level.fire.grabbedNow) level.demolish.Grab(grab.body, grab.by);
    for (const entt::entity gone : level.demolish.Destroyed(registry)) Forget(level, gone);
    // A blast within reach of chapter 4's boss burns it, which is the ONLY thing
    // that can hurt it. Done here rather than inside Fire because Fire::Blast
    // reaches what it grabs by BODY, and this boss is a sensor the level never
    // places, so Fire::Find can never hold it. The reach is its own radius and the
    // blast's together, which is the original's sphere against its 220 x 220
    // collider with that box taken as its half extent - darkdragon.json says so.
    if (level.darkDragon.Alive()) {
        const double reach = level.darkDragon.rules.radiusPx + level.fire.rules.blastPx;
        for (const glm::dvec2& blastPx : level.fire.blastsPx) {
            const glm::dvec2 gap = blastPx - level.darkDragon.atPx;
            if (gap.x * gap.x + gap.y * gap.y <= reach * reach) level.darkDragon.burned = true;
        }
    }
    // And chapter 3's, for the same reason, and by the same explode: burnable 1 in
    // ghost.ent's own data, so the grabber's isBreakableOrExplosiveOrBurnable takes
    // it, and burn() sets the flag its SHOOTING arm reads (planning step 117). Its
    // reach is the grabber's box and its ray, not a radius: Ghost::BlastReaches.
    // The ghost's Tick, further down, reads `burned` on the blast's own tick, as the
    // original's static callback runs after the barrel's (ETHScene.cpp:464-491).
    if (level.ghost.Alive()) {
        for (const glm::dvec2& blastPx : level.fire.blastsPx) {
            if (level.ghost.BlastReaches(registry, blastPx, level.fire.rules.blastPx)) level.ghost.burned = true;
        }
    }
    // What a fireball ran into (ETHBeginContactCallback_fireball, listing
    // L85862; Turrets.hpp has the whole decode). A carranca's, a dragon's and a
    // fire diamond's are the same fireball.ent and meet the same things; only
    // killMainCharacter differs, and Turrets::Tick has already judged the player.
    //
    // AFTER Fire::Tick on purpose. The original sets a flag and the bomb's own
    // callback reads it on a later frame, which is the split Fire.hpp keeps: a
    // chain ripples a tick at a time rather than collapsing into one frame.
    {
        std::vector<std::string> spent;
        std::vector<entt::entity> burnedMinions;
        const float radius = Units::ToMetres(level.turrets.rules.radiusPx);
        // isCarranca (#41-50): the carrancas' own bodies are passed by, and the
        // one that spat a fireball holds it at the start of its flight.
        const std::vector<entt::entity> carrancas = level.turrets.Bodies();
        for (const Turrets::Fireball& ball : level.turrets.fireballs) {
            const glm::vec3 centre = Units::ToWorld(ball.atPx.x, ball.atPx.y);
            const Trigger::Circle circle{glm::vec2(centre.x, centre.y), radius};

            bool struck = false;
            // A shock agent is the ONE sensor it acts on, tested by name in the
            // original (#61-119), and it destroys that one and itself with it.
            for (const Fields::Field& field : level.fields.fields) {
                if (field.gone || !registry.valid(field.body)) continue;
                if (!Trigger::Overlaps(registry, field.body, circle)) continue;
                const entt::entity gone = level.fields.Destroy(field.name, registry);
                if (gone != entt::null) Forget(level, gone);
                struck = true;
                break;
            }
            // Then EVERY SOLID BODY it touches: a wall, a closed door, a crate, a
            // stone, a barrel, a minion. Each destroys it (#146-152); what is
            // burnable is then burned (#159-173) and what is explosive asked to go
            // off (#174-188), the original's own burn() and explode(). Every body
            // it overlaps on the tick it dies is one, because Box2D begins a
            // contact for every pair its step finds touching and the callback runs
            // for each. A burning crate and a barrel already asked are still
            // bodies until their own callbacks take them away, so they stop it too.
            for (const entt::entity body : Shot::SolidBodiesIn(registry, ball.atPx, level.turrets.rules.radiusPx,
                                                               carrancas)) {
                struck = true;
                for (Fire::Burnable& burnable : level.fire.burnables) {
                    if (burnable.body == body && !burnable.gone) burnable.burned = true;
                }
                for (Fire::Bomb& bomb : level.fire.bombs) {
                    if (bomb.body == body && !bomb.blown) bomb.requested = true;
                }
                // A minion: minion.ent and ghost_minion.ent are both burnable, and
                // a burned minion's own callback destroys it (Minions::Burn,
                // planning step 98). This is how 3-32's black knight dies by fire,
                // and so how its ghost comes (R4).
                for (const Minions::Minion& minion : level.minions.minions) {
                    if (minion.gone || minion.body != body) continue;
                    // Two fireballs on one minion in one tick are both spent on it.
                    if (std::find(burnedMinions.begin(), burnedMinions.end(), minion.body) == burnedMinions.end()) {
                        burnedMinions.push_back(minion.body);
                    }
                }
            }
            // NOT chapter 3's boss. ghost.ent is a sensor, and the callback returns
            // on any sensor that is not a shock agent (#46-60, #62-121) before it
            // destroys or burns anything: a fireball flies on through the ghost,
            // as F15 filmed it (t1 #307-#326). Until planning step 117 the port
            // burned the ghost with any fireball within 64 px of it, a rule the
            // original does not have; only a barrel's blast hurts it now (above).
            // `destroy`, as the contact callback sets it, so the layer can put
            // small_explosion.ent where the fireball stood (Turrets::State::spentPx).
            if (struck) {
                spent.push_back(ball.name);
                level.turrets.spentPx.push_back(ball.atPx);
            }
        }
        if (!spent.empty()) {
            std::erase_if(level.turrets.fireballs, [&spent](const Turrets::Fireball& ball) {
                return std::find(spent.begin(), spent.end(), ball.name) != spent.end();
            });
        }
        for (const entt::entity gone : level.minions.Burn(registry, burnedMinions)) Forget(level, gone);
    }
    for (const entt::entity gone : level.launchers.Cull(registry)) Forget(level, gone);
    // And a minion the step left in a killer floor. Not a hazard: that floor does
    // nothing to the player, which is the correction hazards.json records.
    for (const entt::entity gone : level.minions.Cull(registry)) Forget(level, gone);
    // The keys, on where the step left whoever carries them. Anything that is a
    // character OR a minion can pick one up, so the carriers are the player and
    // every minion still standing - the first place two of the port's own modules
    // meet. What a keyhole takes away is a door the builder made, so it goes
    // through Forget as a burnt crate does.
    //
    // Each is offered at its entity point, which the key measures from and trails
    // (Carry::Carrier). A minion's is its body's centre less EntityAboveCentrePx,
    // the point its patrol arrives by. The player's is left at its capsule's
    // centre: that body is the Godot project's 20 x 44, whose centre stands 22 over the
    // feet where the original's entity stands 18.1 (dark_mage.ent's compound is
    // minion.ent's), a deviation of the player's body and not decided here.
    {
        std::vector<Carry::Carrier> carriers;
        carriers.reserve(level.minions.minions.size() + 1);
        if (level.player != entt::null) carriers.push_back(Carry::Carrier{level.player, glm::dvec2(0.0, 0.0)});
        const glm::dvec2 minionPoint(0.0, -level.minions.rules.EntityAboveCentrePx());
        for (const Minions::Minion& minion : level.minions.minions) {
            if (!minion.gone && minion.body != entt::null) carriers.push_back(Carry::Carrier{minion.body, minionPoint});
        }
        for (const entt::entity gone : level.keys.Tick(registry, carriers, dt)) Forget(level, gone);
    }
    // 4-19's red key state (ETHCallback_red_key_state, listing L97171), AFTER the
    // keys: a key taken this tick is seen this tick, as the original's static
    // callback runs after the key's own (ETHScene.cpp:464-491). 6000 ms of fade
    // later it destroy()s SeekEntity('breakable_wall'), and the wall's own callback
    // takes it - here Demolish, on the same tick; RedKey.hpp says why the original
    // may take it a frame later.
    {
        const RedKey::State::Turn turn = level.redKey.Tick(level.keys, static_cast<double>(dt) * 1000.0);
        if (turn.destroyWall) {
            level.redKey.wallName = level.demolish.SeekAndDestroy(level.redKey.rules.wallEntity, level.redKey.name);
            for (const entt::entity gone : level.demolish.Destroyed(registry)) Forget(level, gone);
        }
    }
    // And the shock diamonds, which the OTHER of the two carry filters governs:
    // only a character may take one, so the player alone is offered it and every
    // minion is prey instead. What it strikes is destroyed by Minions rather than
    // here - that module walks its own list every frame, and a body destroyed
    // behind its back would still be in it. The prey is still measured from each
    // body's centre, where ETHCallback_shock_diamond ins 421-430 measure from the
    // minion's GetPositionXY, 11.1 higher: a deviation recorded for Diamonds.
    {
        std::vector<Carry::Carrier> carriers;
        if (level.player != entt::null) carriers.push_back(Carry::Carrier{level.player, glm::dvec2(0.0, 0.0)});
        std::vector<entt::entity> prey;
        prey.reserve(level.minions.minions.size());
        for (const Minions::Minion& minion : level.minions.minions) {
            if (!minion.gone && minion.body != entt::null) prey.push_back(minion.body);
        }
        const std::vector<entt::entity> struck = level.diamonds.Tick(registry, carriers, prey, dt);
        for (const entt::entity gone : level.minions.Take(registry, struck)) Forget(level, gone);
    }
    // turnProjectilesIntoFireBalls. While a fire diamond is carried its callback
    // walks every live projectile.ent every frame and calls burnProjectile, which
    // DELETES the shot and puts a fireball where it was, along the direction it
    // was going. So holding one does not change where a portal opens - it means no
    // portal opens at all, and the tap buys a fireball instead.
    //
    // The original needs a guard for this and the port does not. Its projectile
    // could in principle reach its destiny and open a portal in the frame before
    // the conversion catches it, so computePortalFinalPos aims a fire shot at
    // origin + (destPos - origin) * 64 - a point far outside the level, whose only
    // effect is an enormous stored range, because addProjectile normalizes the
    // direction and the 64 cannot move the aim. Here the conversion runs before
    // Portals::Tick advances the flight, so the shot cannot land first by ordering
    // and there is nothing to scale. Diamonds.hpp carries that decode.
    if (level.diamonds.carrierHasFire && level.portals.flight) {
        const Portals::Flight& shot = *level.portals.flight;
        // burnProjectile's burn_projectile.ent, at the shot as it is deleted
        // (effects.json burn_projectile): a record for the layer, whether or not
        // a fireball can be made of it below.
        level.diamonds.burnedPx.push_back(shot.atPx);
        const glm::dvec2 along = shot.toPx - shot.atPx;
        const double length = std::sqrt(along.x * along.x + along.y * along.y);
        if (length > 0.0) {
            Turrets::Fireball made;
            made.name = "fire_diamond#" + std::to_string(++level.turrets.converted);
            made.atPx = shot.atPx;
            made.velocityPx = (along / length) * level.turrets.rules.speedPx;
            // addFireball's killMainCharacter, passed CLEAR by burnProjectile: the
            // player cannot be hurt by a shot it fired itself.
            made.killsPlayer = false;
            level.turrets.fireballs.push_back(made);
        }
        level.portals.flight.reset();
    }
    // The shock rings swing, and judge where the step left the player. A poll
    // every frame rather than an entry test, which is what separates one of these
    // from a hazard.
    level.fields.Tick(registry, level.player, dt);
    // Chapter 3's boss. AFTER the blast test above on purpose: a blast that
    // reaches it this tick sets `burned`, and this reads it on the same tick
    // rather than the next.
    //
    // It REPORTS what it wants done rather than doing it - Minions owns minion
    // bodies, Keys owns keys and Diamonds owns diamonds, and each walks its own
    // list every frame.
    if (level.ghost.present) {
        // ONLY the escorts this boss called, and that is not a shortcut. The
        // original's rage arm seeks `minion.ent` by NAME, and what it spawns is a
        // minion.ent; level31b's own patroller is a ghost_minion.ent and is a
        // different entity, so it never satisfies that seek. Counting every
        // minion in the level would count that patroller, the count would never
        // reach zero, and the boss would sit in RAGE_MODE for ever without ever
        // summoning - a deadlock, not a slow fight.
        const std::string escort = level.ghost.name + "#";
        std::size_t standing = 0;
        for (const Minions::Minion& minion : level.minions.minions) {
            if (minion.gone || minion.body == entt::null || !registry.valid(minion.body)) continue;
            if (minion.name.rfind(escort, 0) != 0) continue;
            ++standing;
        }

        // RAGE_MODE's diamond restock asks for a player with hp above 0: alive, in
        // the port's terms, which has no hp but a death (Hazards).
        const bool playerAlive = level.player != entt::null && !level.hazards.playerDied;
        const Ghost::State::Turn turn =
            level.ghost.Tick(registry, level.player, playerAlive, standing, data.motion, dt);

        // addEntityIfItCantBeFound (ins 594-630), on SHOOTING's beat: a barrel bomb
        // at barrel_spawn when the level holds none by that name - the ghost's one
        // weakness, put back by the ghost itself. Built from the port's template
        // table (level31b places no barrel) and taken on by Fire and the portals as a
        // placed barrel is; a restock that cannot be built is counted and said once
        // (Restock::State::refusals).
        const Restock::Rule& barrel = level.ghost.rules.restock;
        if (turn.restock && !level.restock.Found(data.scene, level.built, registry, barrel.search)) {
            LevelBuilder::Options options;
            options.prismDirectory = data.prisms;
            options.roles = &data.roles;
            std::string why;
            if (level.restock.Add(data.scene, data.templates, registry, options, barrel, level.ghost.restockSite,
                                  level.built, why) != entt::null) {
                TakeOnAdded(data, level, level.restock.added.back());
            }
        }

        // The escort, on the patrol the boss resolved when the level was found.
        if (turn.summon) {
            const std::string called = level.ghost.name + "#" + std::to_string(level.ghost.summons);
            const entt::entity body =
                level.minions.Summon(registry, called, level.ghost.summonPx, level.ghost.patrol);
            // A minion is teleportable, so it joins the travellers as one built
            // at the start of the level does.
            if (body != entt::null) level.portals.travellers.push_back(body);
        }

        // The key it drops, which is the only key level31b has: the fight IS the
        // lock. Its colour is key.ent's own, not the keyhole's.
        if (turn.droppedKey) {
            Keys::Key dropped;
            dropped.name = level.ghost.keyName;
            dropped.colour = level.ghost.rules.keyColour;
            dropped.atPx = turn.keyPx;
            dropped.fromPx = turn.keyPx;
            dropped.toPx = turn.keyPx;
            level.keys.keys.push_back(dropped);
        }

        // addEntityIfItCantBeFound: a fire diamond back at its spawn, and only
        // when none is left - on SHOOTING's beat, and on every RAGE_MODE update
        // while the player lives. That is what makes dropping one down the gutter
        // mouth survivable rather than final.
        if (turn.restockDiamond) {
            bool anyLeft = false;
            for (const Diamonds::Diamond& diamond : level.diamonds.diamonds) {
                if (diamond.fire && !diamond.gone) anyLeft = true;
            }
            if (!anyLeft) {
                for (Diamonds::Diamond& diamond : level.diamonds.diamonds) {
                    if (!diamond.fire || !diamond.gone) continue;
                    diamond.gone = false;
                    diamond.owner = entt::null;
                    diamond.atPx = level.ghost.diamondSpawnPx;
                    diamond.fromPx = diamond.atPx;
                    diamond.toPx = diamond.atPx;
                    diamond.sinceAimMs = 0.0;
                    break;
                }
            }
        }

        // The fifth death funnel, as the boss's, the carrancas', the fire's and
        // the rings' are: one death in the port and not six.
        if (level.ghost.playerKilled && !level.hazards.playerDied) {
            level.hazards.playerDied = true;
            level.hazards.killedBy = level.ghost.killedBy;
        }

        // R4, THE OWNER'S STANDING RULING: the black knight's death adds the ghost.
        // ETHCallback_ghost_minion runs ETHCallback_minion (#25), which deletes the
        // knight however it was destroyed - a killer floor, a strike, a burn - and
        // then, finding it not alive (#35-40), adds ghost.ent at the point it read
        // before (#43-78) and deletes the level's entity 1430 (#96-103). AFTER the
        // ghost's Tick, so its first callback is the next tick (ghost.json spawn).
        if (!level.ghost.spawned) {
            for (const Minions::Death& death : level.minions.deaths) {
                if (death.minion >= level.minions.minions.size()) continue;
                if (level.minions.minions[death.minion].entity != level.ghost.rules.spawnedBy) continue;
                level.ghost.Spawn(death.atPx);
                // DeleteEntity(SeekEntity(1430)): level31b's anti_sight_wall, a
                // sensor with no sprite. Its body goes as a crushed platform's does.
                if (!level.ghost.deletesNode.empty()) {
                    const auto found = level.built.entities.find(level.ghost.deletesNode);
                    if (found != level.built.entities.end()) {
                        const entt::entity gone = found->second;
                        if (registry.valid(gone)) registry.destroy(gone);
                        Forget(level, gone);
                        level.built.entities.erase(found);
                    }
                }
                break;
            }
        }
    }
    // Chapter 4's torches. BEFORE Portals::Tick on purpose: lighting one calls
    // killProjectile in the original, so a shot that works a switch is SPENT and
    // must not go on to open a portal on the same tick it was consumed.
    //
    // Torch owns no bodies. The light wall carries metadata/breakable, so it is
    // already a Demolish::Breakable with a body and a box that Demolish walks
    // every frame - the removal is Demolish's own, as a struck minion's is
    // Minions::Take.
    if (!level.torch.lights.empty()) {
        std::optional<glm::dvec2> flightPx;
        if (level.portals.flight) flightPx = level.portals.flight->atPx;
        std::vector<glm::dvec2> fireballPx;
        fireballPx.reserve(level.turrets.fireballs.size());
        for (const Turrets::Fireball& ball : level.turrets.fireballs) fireballPx.push_back(ball.atPx);

        const Torch::State::Turn turn = level.torch.Tick(flightPx, fireballPx, dt);

        // hasProjectileAround accepts a fireball as readily as a portal shot, so
        // either can be the one that is spent.
        if (turn.spentFlight) level.portals.flight.reset();
        if (turn.spentFireball >= 0 && turn.spentFireball < static_cast<int>(level.turrets.fireballs.size())) {
            level.turrets.fireballs.erase(level.turrets.fireballs.begin() + turn.spentFireball);
        }

        if (turn.takeWallAway) {
            const std::string by = level.torch.signal.fromTorch.empty() ? "a torch" : level.torch.signal.fromTorch;
            const entt::entity gone = level.demolish.Break(registry, level.torch.wall.name, by);
            if (gone != entt::null) Forget(level, gone);
        }

        // And the way back: a shot signal rebuilds the wall where it stood. This
        // is the only thing in the port that adds a level's own body mid-run, and
        // it is why AfterStep takes the Data at all.
        if (turn.putWallBack) {
            LevelBuilder::Options options;
            options.prismDirectory = data.prisms;
            options.roles = &data.roles;
            std::string built;
            const entt::entity wall =
                LevelBuilder::BuildEntity(data.scene, level.torch.wall.name, registry, options, level.built, built);
            if (wall != entt::null) level.demolish.Restore(level.torch.wall.name, wall);
        }
    }

    // Chapter 4's boss, and the last of the four. AFTER the torch block above on
    // purpose: the torch is what summons it, and a torch lit this tick arms it on
    // the same tick rather than the next.
    if (level.darkDragon.present) {
        const bool hasTorch = !level.torch.lights.empty();
        const glm::dvec2 torchPx = hasTorch ? level.torch.lights.front().atPx : glm::dvec2(0.0);
        const DarkDragon::State::Turn turn = level.darkDragon.Tick(
            registry, level.player, level.torch.lit > 0, torchPx, hasTorch, level.goals.completed, data.motion, dt);

        // breakDarkDragonWall. The wall carries metadata/breakable, so it is
        // already a Demolish::Breakable with a body - Demolish takes it, as it
        // takes the torch's own wall.
        if (turn.brokeWall && !level.darkDragon.wallNode.empty()) {
            const entt::entity gone = level.demolish.Break(registry, level.darkDragon.wallNode, level.darkDragon.name);
            if (gone != entt::null) Forget(level, gone);
        }

        // PortalManager::killAll, on the tick it is wounded. The budget and the
        // count of portals used are deliberately left alone: taking the portals
        // away is not a refund of what they cost.
        if (turn.killedPortals) {
            level.portals.placed.clear();
            level.portals.flight.reset();
        }

        // Its fireballs, at the player - or, on the tick a wound ends, at the
        // level's torch. Same fireball.ent either way, and this one kills.
        if (turn.fired) {
            Turrets::Fireball made;
            made.name = level.darkDragon.name + "#" + std::to_string(level.darkDragon.fired);
            made.atPx = turn.firePx;
            made.velocityPx = turn.aimPx * level.turrets.rules.speedPx;
            made.killsPlayer = true;
            // The one argument that is not the same for every addFireball caller:
            // this dragon passes 2 and everything else 1, and it scales the
            // entity's first particle system alone (darkdragon.json).
            made.particleScale = level.darkDragon.rules.fireballParticleScale;
            level.turrets.fireballs.push_back(made);
        }

        // addEntityIfItCantBeFound (ins 997-1064), after a shot at the player has
        // left: a barrel bomb at barrel_spawn when the level holds none - its own,
        // or one an earlier shot put back. level31c places ONE and the dragon takes
        // three wounds, so without this the fight ends at the first. Built from the
        // placed barrel's node (darkdragon.json `restock`) and taken on by Fire and
        // the portals as that one was.
        const Restock::Rule& restock = level.darkDragon.rules.restock;
        if (turn.restock && !level.restock.Found(data.scene, level.built, registry, restock.search)) {
            LevelBuilder::Options options;
            options.prismDirectory = data.prisms;
            options.roles = &data.roles;
            std::string why;
            if (level.restock.Add(data.scene, data.templates, registry, options, restock,
                                  level.darkDragon.restockSite, level.built, why) != entt::null) {
                TakeOnAdded(data, level, level.restock.added.back());
            }
        }

        // And the key it drops, which is the only key level31c has: the fight IS
        // the lock, as it is in level31b. Its colour is key.ent's own, carried in
        // darkdragon.json because the key is made at run time and has no node.
        if (turn.droppedKey) {
            Keys::Key dropped;
            dropped.name = level.darkDragon.keyName;
            dropped.colour = level.darkDragon.rules.keyColour;
            dropped.atPx = turn.keyPx;
            dropped.fromPx = turn.keyPx;
            dropped.toPx = turn.keyPx;
            level.keys.keys.push_back(dropped);
        }

        // AND THE PLATFORM THE KEY NEEDS A FLOOR FROM. The original's
        // AddScaledEntity makes a body from the .ent; this port makes a static
        // box with no node behind it, SIZED FROM THE TEMPLATE rather than from a
        // number written here - the level places three of the same entity, and
        // their own CollisionPolygon2D is the shape.
        if (turn.droppedPlatform && level.darkDragon.platformBody == entt::null &&
            !level.darkDragon.platformNode.empty()) {
            if (const Tscn::Node* templateNode = data.scene.FindNode(level.darkDragon.platformNode)) {
                glm::dvec2 offsetPx(0.0);
                glm::dvec2 sizePx(0.0);
                if (LevelBuilder::ShapeBoundsPx(data.scene, *templateNode, offsetPx, sizePx)) {
                    level.darkDragon.platformBody =
                        LevelBuilder::BuildStaticBox(registry, level.darkDragon.name + "#platform",
                                                     turn.platformPx + offsetPx, sizePx, LevelBuilder::Options{});
                }
            }
        }
    }

    // The beholder's rocks, against what the step ran them into.
    for (const entt::entity gone : level.boss.Contacts(registry, level.player, level.portals, dt)) Forget(level, gone);
    level.portals.Tick(registry, dt);
    // Then its turn, on where the portals left things. A rock it drops travels
    // and breaks walls as a thrown one does.
    std::vector<entt::entity> stones;
    stones.reserve(level.demolish.stones.size());
    for (const Demolish::Stone& stone : level.demolish.stones) stones.push_back(stone.body);
    const Boss::State::Turn turn = level.boss.Tick(registry, level.player, stones, dt);
    // A stone it broke on itself, named for the picture while Demolish still
    // knows it (Boss::State::brokenNow): its rolling_stone_destroy.
    for (std::size_t i = 0; i < turn.broken.size() && i < turn.brokenPx.size(); ++i) {
        for (const Demolish::Stone& stone : level.demolish.stones) {
            if (stone.body != turn.broken[i]) continue;
            level.boss.brokenNow.push_back(Launchers::StoneGone{stone.name, turn.brokenPx[i]});
            break;
        }
    }
    for (const entt::entity gone : turn.broken) Forget(level, gone);
    for (const Boss::Rock& rock : turn.dropped) {
        if (level.boss.rock.teleportable) level.portals.travellers.push_back(rock.body);
        if (level.boss.rock.demolisher) level.demolish.stones.push_back(Demolish::Stone{rock.name, rock.body});
    }
    if (turn.buttonRaised) RaiseButton(registry, level);
    // Whatever kills the player, it dies as a hazard kills it.
    if (level.boss.playerKilled && !level.hazards.playerDied) {
        level.hazards.playerDied = true;
        level.hazards.killedBy = level.boss.killedBy;
    }
    if (level.turrets.playerKilled && !level.hazards.playerDied) {
        level.hazards.playerDied = true;
        level.hazards.killedBy = level.turrets.killedBy;
    }
    if (level.fire.playerKilled && !level.hazards.playerDied) {
        level.hazards.playerDied = true;
        level.hazards.killedBy = level.fire.killedBy;
    }
    if (level.fields.playerKilled && !level.hazards.playerDied) {
        level.hazards.playerDied = true;
        level.hazards.killedBy = level.fields.killedBy;
    }
}

void Tick(const Data& data, entt::registry& registry, Level& level, float direction, float dt) {
    BeforeStep(data, registry, level, direction, dt);
    Supersonic::PhysicsSystem::Update(registry, dt);
    AfterStep(data, registry, level, dt);
}

} // namespace MagicPortals::Game
