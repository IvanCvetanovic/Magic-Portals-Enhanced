#pragma once

// Chapter 1's boss: the beholder of level 1-32 (level31), as the original's own
// script plays it. boss.json has every number and the bytes of android_game.bin
// it is decoded from (assets/Beholder.angelscript).
//
// - An adder marker spawns what its `entity` names. level31's names
//   beholder.ent, and the beholder floats where the marker stands.
// - Seeking, it glides toward the player's x and wobbles in height. When the
//   player has stood under it long enough it shuts its eye and throws rocks: a
//   rolling stone appears high over the player every stride. The stride's clock
//   is not reset between rounds, as the original's is not, so a later round's
//   first rock can come early and a round hold four.
// - A rolling stone rising through its radius breaks and takes an hp. At its
//   third hit it is dead. Hurt, it fires rings of spikes, then seeks again. A
//   spike flies through everything and kills a player it lands in.
// - The player dies within its radius.
// - Dead, it bursts, and then the level's button rises to its button_dest: the
//   way to the exit. A player within the blast dies.
// - Its rocks break on anything static they run into, and crush a player they
//   hit hard enough. The original's contact callback does both for every
//   rolling stone; the port gives the crush only to these, and the remaster doc
//   says why.
// - What is judged by touching - a rock against the static world or the player
//   - is judged after the step, by the port's own overlap tests, as the demolish
//   and trigger rules are: where the rock is, and where it would have got to had
//   nothing stopped it. The solver can turn a hard hit away within the step, and
//   Box2D's BeginContact, which the original's callback runs on, would still
//   have reported it. A rock a portal holds is taken by it rather than landing.
//
// Not ported: the sounds, the smoke and the explosions' pictures, the final
// blast's line of sight, and the blast's effect on anything but the player. The
// earthquakes are the layer's, read from `hits`, `gone` and `rocksBroken`
// (earthquake.json).

#include "sim/Launchers.hpp"
#include "sim/Portals.hpp"
#include "sim/Roles.hpp"
#include "sim/Tscn.hpp"

#include <optional>
#include <string>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace MagicPortals::Boss {

// BEHOLDER_STATE_SEEKING, THROW_ROCK, GOT_DAMAGE and DEAD: 0 to 3.
enum class Phase : int { Seeking = 0, ThrowRock = 1, GotDamage = 2, Dead = 3 };

// The port's boss.json.
struct Rules {
    std::string entity; // what an adder names for the port to play it
    int maxHp = 0;
    double radiusPx = 0.0;
    // seeking
    double stepPx = 0.0;
    double wobblePx = 0.0;
    double glideMs = 0.0;
    double retargetMs = 0.0;
    double nearPx = 0.0;
    double holdMs = 0.0;
    // throwing rocks
    double throwMs = 0.0;
    double rockStrideMs = 0.0;
    double rockHeightPx = 0.0;
    std::string rockEntity;
    double bobRadS = 0.0;
    double bobPx = 0.0;
    // hurt
    double hurtMs = 0.0;
    double volleyMs = 0.0;
    double spikeStepDeg = 0.0;
    double spikeTurnDeg = 0.0;
    double spikeSpeedPx = 0.0;
    // dead
    double deadMs = 0.0;
    double blastPx = 0.0;
    // its spikes
    double spikeHitShare = 0.0;
    glm::dvec2 characterPx{0.0};
    glm::dvec2 cullMarginPx{0.0};
    // its rocks
    double crushIntensity = 0.0;
    double pxPerUnit = 0.0;       // _guess: the unit
    double contactMarginPx = 0.0; // _guess: how near counts as touching
};

bool LoadRules(const std::string& path, Rules& out, std::string& error);

// Whether the port plays a boss_spawn node: an adder naming boss.json's entity.
bool Plays(const Rules& rules, const Tscn::Node& node);

// The original's PositionInterpolator, with smoothEnd, sin(v * pi/2), as its
// filter.
struct Glide {
    glm::dvec2 fromPx{0.0};
    glm::dvec2 toPx{0.0};
    double timeMs = 0.0;
    double elapsedMs = 0.0;

    glm::dvec2 AtPx() const;
};

struct Beholder {
    std::string name; // the adder it came from
    glm::dvec2 atPx{0.0};
    double startHeightPx = 0.0;
    Phase phase = Phase::Seeking;
    int hp = 0;
    double elapsedMs = 0.0;     // in this phase
    double approachMs = 0.0;    // the player within near_px of its x
    double volleyClockMs = 0.0; // multiSpikeTimer
    double deadMs = 0.0;
    bool alternate = false; // alternateSpikes: the next ring is turned
    // followUp
    bool gliding = false;
    Glide glide;
    double switchMs = 0.0;
    // linearMotion
    glm::dvec2 bobFromPx{0.0};
    double bobAngle = 0.0;
    // overTimeEntityAdder's clock for its rocks, made on the first throw
    bool rockClockMade = false;
    double rockClockMs = 0.0;
    double pulseMs = 0.0; // blinkElapsedTime, which the picture's pulse runs on
    int frame = 0;        // 0 the eye open, 1 shut
    bool gone = false;
    int hits = 0;
    int rocksThrown = 0;
    int volleys = 0;
};

// A rock the beholder dropped: a rolling stone.
struct Rock {
    std::string name; // "<adder>#<n>", counting from 1
    entt::entity body = entt::null;
    glm::dvec2 atPx{0.0};       // as the step began
    glm::dvec2 velocityPx{0.0}; // as the step began
};

// A rock Contacts broke on something static, and the static it judged that on
// (the first it found): the contact whose begin-contact callback runs its first
// arm (ETHBeginContactCallback_rolling_stone #58-71, a shake and
// playStoneHitSound(hitIntensity / 3) above 2) before destroyOnStaticHit's
// (#170-208). The rock's body is gone; its id is kept for the layer's impact
// watch, which read what the rock carried into the step (planning step 171).
struct StaticHit {
    entt::entity rock = entt::null;
    entt::entity other = entt::null;
    glm::dvec2 otherAtPx{0.0}; // the other's entity point: its transform, as the watch reads one
};

struct Spike {
    glm::dvec2 atPx{0.0};
    glm::dvec2 directionPx{0.0}; // a unit vector, +y down
};

struct State {
    Rules rules;
    std::optional<Beholder> beholder;
    Launchers::Throwable rock;    // what its rocks are, from launchers.json
    double contactMarginPx = 0.0; // boss.json's, for touching
    std::vector<Rock> rocks;
    std::vector<Spike> spikes;
    std::string buttonNode; // the level's button, which rises when it dies
    glm::dvec2 buttonDestPx{0.0};
    bool buttonRaised = false;
    glm::dvec2 boundsPx{0.0}; // the level's extent, which its spikes leave
    bool playerKilled = false;
    std::string killedBy;
    int rocksBroken = 0; // on what they ran into
    // Every tick a spike stood in the player's shrunken box, one for each spike:
    // ETHCallback_beholder_spike #178-185 plays playBeholderSpikeHitSound on each
    // such frame, and goes on doing so after the kill. Counted for the layer's
    // sound (planning step 171); nothing in the simulation reads it.
    int spikeHits = 0;
    // Every stone it broke this tick, by name and where it stood: its rocks on
    // what they ran into (Contacts, which empties it first) and a stone that rose
    // into it (Tick's, which the caller names from the stones it handed over).
    // Where ETHCallback_rolling_stone adds rolling_stone_destroy.ent; written for
    // the picture, and nothing in the simulation reads it.
    std::vector<Launchers::StoneGone> brokenNow;
    // Of those, each rock Contacts broke on something static this tick, and on
    // what (Contacts empties it first). For the layer's sound and shake; nothing
    // in the simulation reads it.
    std::vector<StaticHit> staticHitsNow;

    // Before the step: where each rock is and how fast it goes, which its
    // contacts are judged by.
    void BeforeStep(entt::registry& registry);

    // After a step of `dt`, before the portals: a rock that ran into something
    // static breaks, and one that ran into the player hard enough kills it. A
    // rock a portal holds goes through it instead. Returns the rocks broken, for
    // the caller to forget.
    std::vector<entt::entity> Contacts(entt::registry& registry, entt::entity player, const Portals::State& portals,
                                       float dt);

    // After the portals: the spikes fly, and the beholder takes its turn.
    struct Turn {
        std::vector<Rock> dropped;        // new rocks, for the caller to hand to the portals and stones
        std::vector<entt::entity> broken; // stones it broke on itself, for the caller to forget
        std::vector<glm::dvec2> brokenPx; // and where each stood, in `broken`'s order
        bool buttonRaised = false;        // on this tick
    };
    Turn Tick(entt::registry& registry, entt::entity player, const std::vector<entt::entity>& stones, float dt);

private:
    Rock drop(entt::registry& registry, const glm::dvec2& atPx);
    void flySpikes(entt::registry& registry, entt::entity player, float dt);
    void follow(Beholder& beholder, const glm::dvec2& destPx, double ms);
};

// The level's beholder, the button it raises and where to, and the level's
// extent. A level with no beholder gets an empty State. False, with `error`, for
// two beholders, one whose rock launchers.json does not describe, or one in a
// level with no button, button_dest or level_bounds.
bool Find(const Tscn::Scene& scene, const Roles::Table& roles, const Rules& rules, const Launchers::Rules& launchers,
          State& out, std::string& error);

} // namespace MagicPortals::Boss
