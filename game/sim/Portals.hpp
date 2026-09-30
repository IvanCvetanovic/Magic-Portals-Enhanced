#pragma once

// Portals: portal_system.gd's placement and traversal (:69-282), on the transit
// arithmetic in Portal.hpp.
//
// - A tap fires a shot (Shot.hpp) from the player's centre toward the tap. It
//   flies at shot.json's speed, and fails on the first solid body or
//   projectile blocker it meets. Where it arrives a portal opens, a circle,
//   against a budget. The budget is the level's max_portals or portals.json's
//   default, and never more than a pair. At the cap the oldest portal gives
//   way, or with that switched off the shot fails. A shot that arrives inside
//   a no-portal zone fails too. A failed shot costs nothing. One shot flies at
//   a time. A shot that comes near an anti-portal agent on its way dies there
//   and kills it, and the agent's ring goes with it (Agent).
// - A level may also ship static portals (portal_static), which pair by index.
// - Whatever comes into a portal goes out of its partner, as the original sends
//   it (transit.json, decoded): at the partner itself, its velocity turned back,
//   a body's whole and the player's in y only (Portal::ExitVelocity). Placed
//   ends are then spent; static ends stay. For a while after each traversal,
//   entries are ignored.
// - Only the player travels, and bodies whose entity says `teleportable` 1. The
//   inline crate says 0.
// - Entries are per portal and per body, like an Area2D's body_entered. So a
//   portal placed over a crate sees the crate come in on the next tick, and a
//   traveller put down inside its exit does not go back until it has left.

#include "sim/LevelBuilder.hpp"
#include "sim/Motion.hpp"
#include "sim/Mover.hpp"
#include "sim/Portal.hpp"
#include "sim/Roles.hpp"
#include "sim/Shot.hpp"
#include "sim/Trigger.hpp"
#include "sim/Tscn.hpp"

#include <optional>
#include <string>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace MagicPortals::Portals {

// The hard cap on placed portals (portal_system.gd:22-24). A level may allow
// fewer, never more. The owner confirmed it, as for portals.json's default of 2.
inline constexpr int kPairSize = 2;

// portals.json's "placement", "consumption" and "transit" objects, read strictly.
// Their markers differ within a block, so each field says which it has.
struct Rules {
    int defaultMaxPortals = 0; // owner-confirmed, despite its block's _guess

    // TWO RADII, because the original has two and they are unrelated. The port
    // carried one - the Godot project's collision_radius_px, a guess of 16 - and used
    // it for both, which made every no-portal field a quarter of its size.
    // Both of these are decoded, from the port's own placement.json.
    //
    // A placed portal's own circle: g_portalCollisionRadius, 14.
    double entryRadiusPx = 0.0;
    // And the circle an antiportal refuses a tap in, TIMES the node's own scale:
    // half of white_ring.png's 128 px frame, because the original's test is
    // GetSize().x * 0.5 on the field entity itself. placement.json says why.
    double antiportalRadiusPx = 0.0;
    // The entity AntiPortalManager collects (either spelling) and the Scale it
    // gives each one it collects (placement.json `antiportal.manager`, decoded):
    // the ring is drawn at white_ring.png's 128 units x this x the node's own
    // scale, 64 x scale, and the original refuses a tap in half of that. What the
    // port plays is still antiportalRadiusPx; only the picture reads these.
    std::string antiportalEntity;
    double antiportalManagerScale = 0.0;

    // How long a tap is refused for, in milliseconds, both decoded. The Godot project
    // carries 0.0 s and 0.25 s and says of them "the names are real; the numbers
    // are invented".
    //
    // firstMs is from the START OF THE LEVEL and gates the tap path entirely;
    // nextMs is from the last ACCEPTED tap. placement.json says why the second
    // is spent even by a shot that goes on to fail.
    double firstPortalMinMs = 0.0;
    double nextPortalMinMs = 0.0;
    // How far a touch may stray from where it went down and still fire when it comes
    // up: hasPortalRequestInput's scale(18), in units of the 256-unit view
    // (placement.json `release`, decoded). TrackTouch below says when.
    double releaseMaxMoveUnits = 0.0;

    bool consumeOnTraverse = false;    // owner-confirmed
    bool staticPortalsPersist = false; // owner-confirmed 11 September: they stay
    bool recycleOldestAtCap = false;   // _guess
    Portal::Transit transit;           // _guess, every field

    // AND WHAT ETHCallback_portal DOES TO A PORTAL ONCE IT IS THERE, both decoded
    // (placement.json `portal`, the visuals plan's 7.4).
    //
    // It perishes: 20,000 ms of the level's own clock after it opened, unless the
    // entity says dontPerish, which nothing in the game ever says. That is the
    // owner's standing ruling R14, taken as the original plays it. Zero is off,
    // stated rather than implied, so a suite that wants a portal to stay can say so.
    double perishMs = 0.0;
    // And it HOVERS: linearMotion(thisEntity, vertical, 0, 0) every frame, with
    // portal.ent's own speed 1 and stride 2, so it rides 2 units up and down on a
    // 6.28 s cosine from where the shot opened it. One point, not two: the call
    // ends in SetPosition, which is the point a traveller enters by.
    Motion::Row bob;
    // linearMotion's shared arithmetic (motions.json), which Game::LoadData hands
    // over: the frame cap its step is taken over and the turn it wraps at.
    Motion::Rules motion;

    // WHAT THE ORIGINAL WORKS OUT AT THE TAP (placement.json final_pos, decoded):
    // computePortalFinalPos's ray and its head-height retry, which decide the
    // mark a tap leaves - red_cross or portal_mark - and red_waves (A). Read for
    // the PICTURE only (the owner's standing ruling R15): where a shot goes and
    // how it ends is still Tick's.
    struct FinalPosRules {
        double rayStartUnits = 0.0;          // the ray starts this far out along the aim
        std::vector<std::string> rayIgnores; // the entity names it passes through, exactly as spelled
        double pushUnits = 0.0;              // jumpForth: past a body it may open on, short of one it may not
        glm::dvec2 retryFromUnits{0.0};      // the retry's origin, from the caster's
        double retryBackUnits = 0.0;         // and how far back off its aim it starts
    };
    FinalPosRules finalPos;
    // The entity whose presence turns a budget-0 tap into red_waves and a shake
    // (placement.json no_budget, blockPortalEffects' cached SeekEntity).
    std::string noPortalSignEntity;

    // WHAT KILLS AN ANTI-PORTAL AGENT, AND WHICH RING GOES WITH IT (placement.json
    // anti_portal_agent, decoded; the remaster record's step 126). A shot still in
    // flight within killShare x the agent's size dies with it, and the agent then
    // deletes the first ring spelled exactly ringEntity whose field holds it: in
    // its own bucket, then in each of seekNeighbours' in turn.
    struct AgentRules {
        double sizeUnits = 0.0;  // GetSize().x: anti_portal_agent.png's 32, times the node's scale
        double killShare = 0.0;  // 0.7
        std::string ringEntity;  // "antiportal", the one spelling the search compares
        double bucketUnits = 0.0; // GetBucket's floor(pos / 256)
        std::vector<glm::dvec2> seekNeighbours;
    };
    AgentRules agent;
};

// Enhanced's portals.json: its placement, consumption and transit objects.
// Its collision_radius_px is read and checked so a malformed file is still
// refused, but nothing plays by it - LoadPlacement replaces it.
bool LoadRules(const std::string& path, Rules& out, std::string& error);

// The port's own placement.json, over the top: the two decoded radii, and the
// antiportal manager's entity and Scale. All must be there, the numbers above
// zero. A missing one is an error rather than a default, because a default
// here would be a guess written into code - which is the mistake this file
// exists to undo.
bool LoadPlacement(const std::string& path, Rules& out, std::string& error);

// An antiportal. A tap within collision_radius_px times its `scale` is refused
// (portal_system.gd:136-146). A zone with a speed and a stride patrols: it swings
// as a moving platform does, on the axis its `direction` names, vertical where it
// names none (behaviours.gd:48-51, 113-127). No placed zone of the game does
// since the anti_portal_agents stopped being zones (Agent, below).
//
// A zone an agent's death deleted is `gone`, and refuses nothing: the original's
// isPointInAntiPortalField returns false for an entity that is not IsAlive.
struct NoPortalZone {
    glm::dvec2 centrePx{0.0}; // where the level places it
    double scale = 1.0;
    bool moving = false;
    Mover::Oscillation motion; // when it patrols
    std::string name;          // last, so {centre, scale} still makes a zone
    // An EXPLICIT radius, for a zone that is not an antiportal the level placed.
    // Zero means the old arithmetic - antiportalRadiusPx times this node's own
    // scale - which is every antiportal in the game. Nothing the port builds sets
    // it now: the gravity wells' zones, which did, are gone (00_order R2, the
    // remaster doc's step 125), since the original's well refuses no portal.
    double radiusPx = 0.0;
    // The node's entity name as the level spells it, antiportal or antiportal.ent:
    // an agent's death deletes only the bare spelling (Rules::AgentRules).
    std::string entity;
    bool gone = false; // deleted by an agent's death: it refuses nothing, and is drawn no more

    glm::dvec2 CentreNowPx() const { return moving ? motion.AtPx() : centrePx; }
};

// AN ANTI-PORTAL AGENT: the red orb that holds a ring open (placement.json
// anti_portal_agent; ETHCallback_anti_portal_agent, bytes 336011..337087). It
// refuses no portal and has no body. A portal shot that comes within
// killShare x its size while still in flight dies there, and so does the agent,
// and the agent takes one ring with it (State::Tick). One with a speed and a
// stride patrols as a zone once did (Mover::Oscillation, the port's swing: R7 is
// not built): two in level10 and one in level13b, each inside its own ring's
// field all the way.
struct Agent {
    std::string name;
    glm::dvec2 centrePx{0.0}; // where the level places it
    double scale = 1.0;       // its node's `scale`: none of the 21 carries one
    bool moving = false;
    Mover::Oscillation motion; // when it patrols
    bool gone = false;         // a shot killed it

    glm::dvec2 CentreNowPx() const { return moving ? motion.AtPx() : centrePx; }
};

// An agent a shot killed this tick, for the frame to draw and play: the
// explosion, the shake and the sound are the layer's.
struct AgentKill {
    std::string agent;
    glm::dvec2 atPx{0.0};   // the agent's point: pos, where the explosion is added
    glm::dvec2 shotPx{0.0}; // where the shot first came within reach of it
    std::string ring;       // the zone deleted with it, or empty when none held it
};

struct Placed {
    // WHAT NAMES IT, for as long as anything remembers it - the simulation's own
    // list, the layer's drawing of it, and a traversal's record. It is NOT the
    // count of portals used: see State::nextPortalId.
    int id = 0;
    // WHERE IT STANDS THIS TICK, which is where the shot opened it plus its hover
    // (rules.bob). Everything plays by this one point, the circle below included,
    // because the original's linearMotion moves the portal entity itself and
    // PortalManager::doTeleporting reads that same position.
    glm::dvec2 atPx{0.0};
    glm::dvec2 basePx{0.0}; // linearMotion's originalPos: where the shot opened it
    Motion::Linear bob;     // its hover, as the last tick left it
    // ETHCallback_portal's elapsedTime, in milliseconds of the level's clock: the
    // FIRST tick writes 0 rather than adding, as #22-53 do, and the portal is
    // killed once this is past rules.perishMs.
    double ageMs = 0.0;
    bool aged = false;
    Trigger::Circle trigger;
    std::vector<entt::entity> inside; // the travellers overlapping it as of the last tick
    // Whether a pair ever carried anything through it: the original's
    // hasTeleportedSomething, which PortalManager::teleportToOther (bytes
    // 144944..145873) sets on both ends of a placed pair. Only KillAll reads it.
    bool teleported = false;
};

// A pre-placed portal (portal_system.gd:79-87, 206-227). `destiny` is the index
// it sends a traveller to. When no static portal has that index the traveller
// goes to a placed portal instead: level1 ships one static portal and grants one
// placement, and the placement is its partner (docs/original-gameplay.md 2.3).
//
// That partnership runs both ways, which the port missed until the game was
// played: entering the lone placement sends the traveller to the first static
// portal, as PortalManager::teleportToFirstStaticPortal does on a level whose
// maxPortals is 1 (Portals.cpp, where Tick pairs an end with its exit).
struct Static {
    std::string name;
    int index = 0;
    int destiny = 0;
    bool hasDestiny = false;
    std::string colour;   // "red" or "blue", as the level says; for drawing
    glm::dvec2 atPx{0.0}; // the node's position: where a traveller comes out
    Trigger::Box trigger; // its trigger_size box: where one goes in
    bool live = true;     // false once spent, when static portals do not persist
    std::vector<entt::entity> inside;
};

// A projectile blocker: anti_projectile_wall.ent, an invisible box a portal shot
// dies in (ETHCallback_anti_projectile_wall). Its box is its trigger_size, as
// the Godot project's ProjectileBlocker reads it. The player walks through it.
struct Blocker {
    std::string name;
    Trigger::Box box;
};

// A reflector: reflect_agent, a marker with no body that a portal shot comes
// off, mirrored across its plane (the owner, 11 September). How near a shot has
// to pass, and how many reflectors one shot comes off, are shot.json's guesses.
// The shot goes on for the rest of the distance it was fired, so a flat
// reflector sends it to the tap mirrored across it.
struct Reflector {
    std::string name;
    glm::dvec2 centrePx{0.0};
    bool vertical = false; // its plane: a vertical one turns a shot's x back, a horizontal one its y
};

// A traversal this tick: the three points the original's teleport has in hand
// when it calls vanishEffect and appearEffect (Portal.angelscript), and nothing
// else.
//
// PortalManager::teleportToOther (bytes 144944..145873) calls vanishEffect(#61)
// with the traveller's position BEFORE #69 moves it and with the portal it went
// in by, and appearEffect(#71-75) with the portal it comes out of;
// teleportToFirstStaticPortal (#44, #58) and ETHCallback_portal_static (#144,
// #165) call the same pair. So one record covers every route a traveller takes,
// which is what the port's one Tick loop already is.
//
// Cleared and refilled by every Tick, as Fire::State::blastsPx and
// Turrets::State::spentPx are. Nothing in the simulation reads it: the layer
// reads it on the tick that fills it (latchSimEffects) and adds the teleport's
// three effects on the next frame (effects.json's suck_effect, sparkles and
// appear_effect rows).
struct Traversal {
    // vanishEffect's objectPos: where the traveller stood, its own point. The
    // port's is its body's centre, which for the PLAYER is 22 units over its feet
    // (player.json's capsule, a guess) against the original's character point at
    // 18.1 - the divergence 00_order 8.15 R25 leaves with the player's owner. So
    // the sparkles, and the direction the suck effect is placed along, both sit
    // about 4 units high on a teleporting player.
    glm::dvec2 travellerPx{0.0};
    glm::dvec2 entryPx{0.0}; // vanishEffect's portalPos: the end it went in by
    glm::dvec2 exitPx{0.0};  // appearEffect's argument: the end it came out of
};

// A portal shot on its way.
struct Flight {
    glm::dvec2 fromPx{0.0}; // the player's centre when it was fired
    glm::dvec2 toPx{0.0};   // where it opens a portal: the tap, mirrored by each reflector it came off
    glm::dvec2 atPx{0.0};   // where it is now
    int reflections = 0;    // how many reflectors it has come off
    int lastReflector = -1; // the last of them, which cannot turn it again at once
    // Whether this leg has had its first tick - ETHCallback_projectile's first
    // frame, which flashes projectile_shoot - since it was fired or last turned.
    // Read for the picture alone (State::launches).
    bool launched = false;
};

// WHAT computePortalFinalPos ANSWERS: the global (bytes 355236..356384) and
// PortalManager's method with its head-height retry over it (141090..142200),
// run on the port's own bodies and fields (placement.json final_pos). What it
// gives is PortalPosition's pos and closestEntity, *hasFailed - the shot's
// willDie, so red_cross or portal_mark - and whether it added red_waves (A).
// READ FOR THE PICTURES ONLY, by the owner's standing ruling R15: where the
// port's shot flies and how it ends is still Tick's, and the two can disagree
// where the port's flight is not the original's (R23, not built).
struct FinalPos {
    glm::dvec2 atPx{0.0};              // PortalPosition.pos: where the original would send its shot
    bool willDie = false;              // *hasFailed
    bool redWaves = false;             // the ray met nothing and the aim is in a field: red_waves (A) there
    bool preferred = false;            // *preferedTeleportEntity was set: it ended on a body a portal may open on
    entt::entity closest = entt::null; // PortalPosition.closestEntity: what the ray met first
    bool retried = false;              // the method's head-height retry ran
};

// WHY A TAP DID WHAT IT DID - the refused-tap record (00_order 8.13's 10.1 row).
// Written by every Shoot call, whatever it decides, and read by the layer the
// moment the call returns. The reasons are the original's, in its own order:
// PortalManager::update returns before either tap path while the level is
// younger than first_ms (TooEarly); a budget of 0 goes to blockPortalEffects
// (NoBudget: red_waves (B) where the sign is); managePortalInsertion then holds
// a tap off for next_ms, drawing nothing (Cooldown), and shows the hourglass for
// one that passes while a shot still flies (InFlight). The port's own refusals
// and their results are unchanged - Shoot returns what it returned, and neither
// clock moves on a refusal: the original's hourglass tap restarts the 400 ms,
// which is 00_order 8.15's R23 and not played here.
//
// field_clear_ray, 8.13's third reason, is not a refusal in either game: the
// shot fires, and computePortalFinalPos adds red_waves (A) for it - and for a
// reflected shot too. So it rides on the FinalPos a fired tap (and a
// reflection) is given, not on this.
enum class TapReason { Fired, NoShooter, TooEarly, NoBudget, Cooldown, InFlight };
struct Tap {
    glm::dvec2 atPx{0.0};
    TapReason reason = TapReason::Fired;
};

// THE TOUCH A SHOT IS FIRED BY (placement.json `release`, decoded). The original
// fires on the frame a touch comes UP, never on the one it goes down, and only when
// it has not strayed more than scale(18) from where it went down: TouchInputManager
// keeps, per touch, where it went down (hitPos), where it was last held (lastTouchPos)
// and the farthest it has been from hitPos (maxMoveLength), and
// hasPortalRequestInput asks for GSKS_RELEASE with that farthest at most scale(18).
// The shot is aimed at the last point the touch was HELD at (getReleasePos), mapped
// through the camera of the release frame - not at where it came up.
//
// One of these follows the layer's one pointer, tick by tick, whatever it is: a real
// press or a tap a capture scheduled.
struct TouchTrack {
    bool down = false;
    glm::dvec2 hitView{0.0};   // hitPos: where it went down, in units of the view
    glm::dvec2 lastView{0.0};  // lastTouchPos: where it was on the last tick it was held
    glm::dvec2 lastPx{0.0};    // the same point in the viewport's pixels, which the shot maps
    double maxMoveUnits = 0.0; // maxMoveLength: the farthest it has been from hitView
};
// What a tick's touch did to it: whether this is the tick it came up on, with the
// last point it was held at and how far it strayed.
struct TouchRelease {
    bool released = false;
    glm::dvec2 atView{0.0};
    glm::dvec2 atPx{0.0};
    double maxMoveUnits = 0.0;
};
// One tick of TouchInputManager::update for the one pointer, given the tick's
// edges and where it is. A touch that goes down (re)starts the record; one held
// moves its last point and may stretch its move; the tick it comes up returns its
// record and ends it. A press and a release on the same tick are a touch that went
// down and up there. Off the game (`over` false), or up with no release this record
// saw, the touch is forgotten and fires nothing.
TouchRelease TrackTouch(TouchTrack& track, bool pressed, bool held, bool released, bool over,
                        const glm::dvec2& atView, const glm::dvec2& atPx);
// hasPortalRequestInput: this tick's touch came up, and strayed no more than the
// rules allow. What it then does - the gates, the controls, the shot - is the
// caller's and Shoot's.
bool RequestsPortal(const Rules& rules, const TouchRelease& release);

// What a tick's shot did, for the frame to draw: cleared and refilled by every
// Tick, as `transits` is, and read by nothing in the simulation.
//
// A leg's first tick: ETHCallback_projectile's first frame, projectile_shoot.
struct Launch {
    glm::dvec2 fromPx{0.0};   // where the leg starts: the shooter's centre, or where it was turned
    glm::dvec2 towardPx{0.0}; // where it is going
};
// A turn off a reflector: ETHCallback_reflect_agent, which runs
// computePortalFinalPos for the new leg as a reflected shot (so a wall does not
// doom it), marks its end and flashes where the old one was.
struct Reflection {
    glm::dvec2 atPx{0.0}; // where it turned
    glm::dvec2 toPx{0.0}; // the new destination, before the original would clip it
    FinalPos said;
};
// How a shot ended without a portal, and where.
enum class FailKind {
    Body,    // the first solid body on its way
    Blocker, // an anti_projectile_wall's box (ETHCallback_anti_projectile_wall)
    Refused, // it arrived where TryPlace opens nothing: a no-portal zone
};
struct Failure {
    glm::dvec2 atPx{0.0};
    FailKind kind = FailKind::Body;
    bool inField = false; // the point is inside a zone the port plays (InField)
};

struct State {
    Rules rules;
    Shot::Rules shot;
    int budget = 0;
    entt::entity shooter = entt::null; // the player: where a shot leaves from, and what it passes through
    // What else a flying shot passes through: each gravity well's agent, set by
    // Game::Start (00_order R2). The original's projectile.ent has no shape and
    // meets nothing on its way, and the ray that judges its tap passes
    // gravity_agent by name, so a well stops no shot.
    std::vector<entt::entity> shotPasses;
    std::vector<NoPortalZone> zones;
    std::vector<Agent> agents;
    std::vector<Blocker> blockers;
    std::vector<Reflector> reflectors;
    std::vector<entt::entity> travellers; // the player, and every teleportable body
    std::vector<Static> statics;          // in the level's order
    std::vector<Placed> placed;           // oldest first
    int portalsUsed = 0;                  // every portal placed: what the golden score counts
    // AND WHAT NAMES ONE, which is a different thing and has to be. The original
    // keeps an array of ETHEntity HANDLES, and a handle is never handed out
    // twice; the count above is the score's, and killAll(true) gives it back for
    // every portal that carried nothing (PortalManager::killAll #47-72, which
    // calls decrementPortalCounter and pops nothing off the array).
    //
    // Naming a portal by the refundable count let the clear-portals button hand a
    // new portal the id of one whose entity was still fading out its 400 ms, so
    // the new portal wore the dying one's colour and burst with it. This counter
    // is never given back, so an id is a portal's for the level's life.
    int nextPortalId = 0;
    int traversals = 0;
    std::vector<Traversal> transits; // what went through this tick, for the frame to draw
    float lockoutS = 0.0f;
    std::optional<Flight> flight;
    int shotsFired = 0;
    // A shot an agent killed is not one of these: the original's projectile is
    // deleted by the agent's callback, so its own never fails it (no portal_fail,
    // no failed sound). agentsKilled counts those.
    int shotsFailed = 0;
    int agentsKilled = 0;
    int reflections = 0;     // every time a shot came off a reflector
    std::string lastFailure; // what stopped the last failed shot

    // THE TWO COOLDOWN CLOCKS, counting UP in milliseconds as the original's two
    // Timers do - gameTimer and lastPortalTimer - so the comparisons here read
    // as its own do rather than inverted into countdowns.
    //
    // sinceStartMs is the level's age; sinceTapMs the time since the last tap
    // this took. BOTH START AT ZERO, as the original's do: Timer's constructor
    // calls its own reset, and a PortalManager - with both of its Timers - is
    // built per level.
    //
    // So the first tap of a level is held off by the LARGER of the two, which is
    // nextPortalMinMs at 400 rather than firstPortalMinMs at 300. That is the
    // original's behaviour and not an accident of this port: its lastPortalTimer
    // is at zero too when a level begins.
    double sinceStartMs = 0.0;
    double sinceTapMs = 0.0;

    // THE ZERO-GRAVITY RECOIL (PortalManager::applyImpulse), set by Game::Start
    // from the level's own `no_gravity`. Named after the original's own member,
    // PortalManager.m_noGravity, which gates the call in exactly this place: a
    // tap that is TAKEN shoves the shooter away from where it aimed, whether or
    // not the shot goes on to open a portal. Zerog.hpp holds the decode.
    bool noGravity = false;
    double recoilMps = 0.0;

    // THE PICTURES' RECORDS (the visuals plan's 10.1), none of which the
    // simulation reads. The bodies the tap-time ray passes through by name
    // (Find, from rules.finalPos.rayIgnores); whether the level places the
    // no-portal sign (Find, rules.noPortalSignEntity); what the last Shoot call
    // did with its tap; and what this tick's shot did (Tick).
    std::vector<entt::entity> rayIgnored;
    bool noPortalSign = false;
    Tap lastTap;
    std::vector<Launch> launches;
    std::vector<Reflection> reflected;
    std::vector<Failure> failures;
    std::vector<AgentKill> agentKills; // this tick's alone, as failures are

    // A tap at a point in the level: a shot fired toward it. False when none
    // goes, because the level allows no portal or a shot is already flying.
    // Either way it writes lastTap, and nothing else it writes on a refusal.
    bool Shoot(entt::registry& registry, const glm::dvec2& atPx);

    // Whether a point is inside a zone the port plays - the test TryPlace
    // refuses by, exactly: an antiportal's radius times its scale, or a zone's
    // own radius, and never a zone that is gone. It stands in for
    // isPointInAntiPortalField in computePortalFinalPos, so the pictures follow the
    // fields the port plays (R1 is not built).
    bool InField(const glm::dvec2& atPx) const;
    // The same test on one zone: isPointInAntiPortalField, the port's radius.
    bool Holds(const NoPortalZone& zone, const glm::dvec2& atPx) const;

    // deleteAntiPortalInThisBucket(GetBucket(atPx), atPx, true), what an agent's
    // death calls (rules.agent): the first live zone spelled rules.agent.ringEntity
    // whose field holds the point, in the point's bucket and then its neighbours',
    // is gone. Its name, or empty when none holds the point.
    std::string DeleteRingAt(const glm::dvec2& atPx);

    // How near a shot an agent kills it: killShare x sizeUnits x its scale.
    double AgentReachPx(const Agent& agent) const;
    const Agent* FindAgent(const std::string& name) const;

    // The global computePortalFinalPos (355236..356384) from `casterPx` toward
    // `destPx`, with its hasBeenReflected. Reads the registry and
    // changes nothing.
    FinalPos FinalPosFrom(entt::registry& registry, const glm::dvec2& destPx, const glm::dvec2& casterPx,
                          bool hasBeenReflected) const;

    // PortalManager::computePortalFinalPos (141090..142200) for a tap at `destPx`
    // from a shooter at `casterPx`: the global, then the head-height retry when
    // the aim is outside every field. Not for a tap with a fire diamond carried,
    // whose method never casts a ray (FireDiamondWillDie).
    FinalPos Predict(entt::registry& registry, const glm::dvec2& casterPx, const glm::dvec2& destPx) const;

    // The fire diamond's tap (the method's #21-64): no ray, and *hasFailed never
    // written, so willDie is the caller's leftover - the low byte of absTouchPos.x
    // as a float (managePortalInsertion #108-109 RDR4, read back by CpyVtoR4 and
    // JLowZ, as_context.cpp:3770-3775). `touchXScreenPx` is that x in the
    // original's world pixels: the level's units times its scale factor.
    static bool FireDiamondWillDie(double touchXScreenPx);

    // A portal opened at a point at once, as an arriving shot opens one. The
    // suites use it to put portals where a test needs them. False when refused.
    bool TryPlace(const glm::dvec2& atPx);

    // PortalManager::killAll (bytes 147664..148016): every placed portal goes at
    // once. With `refund`, each that never carried anything is given back to the
    // count the golden score reads (GameStateController::decrementPortalCounter),
    // which is what the clear-portals button asks for. A shot still in flight is
    // not a portal yet and is left alone, as killAll leaves it. Returns how many
    // went.
    int KillAll(bool refund);

    const NoPortalZone* FindZone(const std::string& name) const;

    // One tick, after the physics step. The lockout runs down, a patrolling zone
    // or agent swings on, a shot flies on - and dies with the first live agent it
    // comes within reach of while still in flight - and a traveller newly inside
    // a portal goes out of its partner.
    void Tick(entt::registry& registry, float dt);

    // A placed portal's entry circle, put back over its point: the hover moves
    // both, because the original has one position and not two.
    static void Centre(Placed& portal);
};

// A built level's portals, no-portal zones, anti-portal agents and projectile
// blockers. A patrolling agent swings at movers.json's rate scale, and the
// player's shots fly by shot.json. False, with `error`, for data that does not read.
bool Find(const Tscn::Scene& scene, const Roles::Table& roles, const LevelBuilder::Built& built,
          entt::registry& registry, entt::entity player, const Rules& rules, const Mover::Rules& movers,
          const Shot::Rules& shot, State& out, std::string& error);

} // namespace MagicPortals::Portals
