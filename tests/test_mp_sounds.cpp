// The port's sounds: the original's AudioManager, decoded and carried as data.
//
// Two halves, like test_mp_sprites. The reader is tested on files this suite
// writes, and on the port's own sounds.json, which is IN this repository - so
// that part runs anywhere. Only the last case opens the original's own mp3s,
// and it says where it looked when they are not there.
//
// What is pinned here is the decode, not a preference. Every name and number
// below was read out of android_game.bin's AudioManager (the byte offsets are
// in docs/planning/2026-09-11-magic-portals-remaster.md), and several of them
// are surprising enough that a future reader would "fix" them without a test
// saying otherwise: the crystal VANISH sound is spike_hit, unlocking a door is
// a crystal_gather at half speed, and a menu button is the only thing in the
// game that plays button.mp3.

#include "TestHarness.hpp"
#include "MpSuite.hpp"

#include "sim/Sounds.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

using namespace MagicPortals;

namespace {

const std::string kOriginal = MAGICPORTALS_ORIGINAL_DIR;
const std::string kPortData = MAGICPORTALS_PORT_DATA_DIR;

std::filesystem::path Scratch() {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "supersonic-test-mp-sounds";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}

// A sounds.json with `body` as its hooks, music and events.
std::string WriteRules(const char* name, const std::string& body) {
    const std::filesystem::path path = Scratch() / name;
    std::ofstream file(path, std::ios::trunc);
    file << body;
    file.close();
    return path.string();
}

// The smallest file the reader accepts, for the refusal cases to vary.
std::string Whole(const std::string& hooks, const std::string& events = "{}") {
    return "{\"hooks\":" + hooks +
           ",\"music\":{\"menu\":{\"file\":\"warlords_loop_a.mp3\",\"volume\":1.0,\"loop\":true}}"
           ",\"events\":" + events + "}";
}

void TheReaderRefusesWhatWouldBeSilent() {
    Sounds::Rules rules;
    std::string error;

    // Two files and no word on whether they play together or one at random:
    // the difference between an explosion and a crystal, and unguessable.
    const std::string twoFiles =
        WriteRules("two-files.json", Whole("{\"h\":{\"files\":[\"a.mp3\",\"b.mp3\"]}}"));
    CHECK_MSG(!Sounds::LoadRules(twoFiles, rules, error), "two files with no pick should be refused");
    CHECK_MSG(error.find("both or one") != std::string::npos, "the refusal should say what is missing: " + error);

    // An event naming a hook that is not there. This is THE bug this reader
    // exists to catch: a typo in an event's hook is a sound nobody ever hears,
    // and nothing at runtime would ever say so.
    const std::string typo =
        WriteRules("typo.json", Whole("{\"playDeathSound\":{\"files\":[\"a.mp3\"]}}",
                                      "{\"player_died\":\"playDeethSound\"}"));
    CHECK_MSG(!Sounds::LoadRules(typo, rules, error), "an event naming no hook should be refused");
    CHECK_MSG(error.find("playDeethSound") != std::string::npos, "the refusal should name the typo: " + error);

    // A speed that is none of the three shapes the original uses.
    const std::string speed =
        WriteRules("speed.json", Whole("{\"h\":{\"files\":[\"a.mp3\"],\"speed\":\"quickly\"}}"));
    CHECK_MSG(!Sounds::LoadRules(speed, rules, error), "an unreadable speed should be refused");

    // A rate limit with no timer to keep it on: the original's limits are per
    // shared Timer, and which hooks share one is the whole point.
    const std::string noTimer =
        WriteRules("no-timer.json", Whole("{\"h\":{\"files\":[\"a.mp3\"],\"min_interval_ms\":50}}"));
    CHECK_MSG(!Sounds::LoadRules(noTimer, rules, error), "a rate limit with no timer should be refused");

    // And an event may be deliberately silent, which is not an error: the port
    // names what it can see happen even where the original has no sound for it.
    const std::string silent =
        WriteRules("silent.json", Whole("{\"h\":{\"files\":[\"a.mp3\"]}}", "{\"button_pressed\":\"\"}"));
    CHECK_MSG(Sounds::LoadRules(silent, rules, error), "a silent event should be allowed: " + error);
    CHECK_MSG(rules.ForEvent("button_pressed") == nullptr, "a silent event plays nothing");
    CHECK_MSG(rules.ForEvent("never_happens") == nullptr, "an event nobody named plays nothing");
}

void TheSpeedShapesRead() {
    Sounds::Rules rules;
    std::string error;
    const std::string path = WriteRules(
        "speeds.json",
        Whole("{\"fixed\":{\"files\":[\"a.mp3\"],\"speed\":0.5},"
              "\"drawn\":{\"files\":[\"b.mp3\"],\"speed\":\"random 0.5 to 2.0\"},"
              "\"door\":{\"files\":[\"c.mp3\"],\"speed\":\"3000 / doorOpenStride\"}}"));
    CHECK_MSG(Sounds::LoadRules(path, rules, error), error);

    const Sounds::Hook* fixed = rules.FindHook("fixed");
    const Sounds::Hook* drawn = rules.FindHook("drawn");
    const Sounds::Hook* door = rules.FindHook("door");
    CHECK_MSG(fixed != nullptr && drawn != nullptr && door != nullptr, "all three hooks should read");
    if (fixed == nullptr || drawn == nullptr || door == nullptr) return;

    CHECK_NEAR(static_cast<float>(fixed->speed), 0.5f);
    CHECK_MSG(!fixed->speedIsRandom && !fixed->speedFromDoorStride, "a number is just a number");
    CHECK_MSG(drawn->speedIsRandom, "\"random 0.5 to 2.0\" is a draw");
    CHECK_NEAR(static_cast<float>(drawn->speedFrom), 0.5f);
    CHECK_NEAR(static_cast<float>(drawn->speedTo), 2.0f);
    CHECK_MSG(door->speedFromDoorStride, "the door's speed comes from its own stride");
}

// A hook that hands on its caller's volume, and an impact's gate and volume
// (planning step 142).
void TheArgumentVolumeAndTheImpactsRead() {
    Sounds::Rules rules;
    std::string error;
    const std::string fall = WriteRules(
        "argument.json", Whole("{\"thump\":{\"files\":[\"a.mp3\"],\"volume\":\"argument\"}}", "{\"landed\":\"thump\"}"));
    CHECK_MSG(Sounds::LoadRules(fall, rules, error), error);
    const Sounds::Hook* thump = rules.FindHook("thump");
    CHECK_MSG(thump != nullptr && thump->volumeFromArgument, "\"argument\" is the caller's volume");

    const std::string loud = WriteRules("loud.json", Whole("{\"h\":{\"files\":[\"a.mp3\"],\"volume\":\"loud\"}}"));
    CHECK_MSG(!Sounds::LoadRules(loud, rules, error), "a volume that is neither a number nor \"argument\"");

    // An impact naming an event the table does not have is a contact nobody hears.
    const std::string unknown = WriteRules(
        "impact-typo.json",
        "{\"hooks\":{\"h\":{\"files\":[\"a.mp3\"]}},\"music\":{},\"events\":{\"landed\":\"h\"},"
        "\"impacts\":{\"player\":{\"event\":\"landde\",\"hit_above\":3.5,\"volume_divisor\":18}}}");
    CHECK_MSG(!Sounds::LoadRules(unknown, rules, error), "an impact's event that is not an event is refused");
    CHECK_MSG(error.find("landde") != std::string::npos, "and named: " + error);

    Sounds::Impact crate;
    crate.event = "knock";
    crate.hitAbove = 2.0;
    crate.volumeDivisor = 30.0;
    crate.absolute = true;
    double volume = 0.0;
    CHECK_MSG(Sounds::ImpactVolume(crate, -6.0, volume) && ::test::nearly(static_cast<float>(volume), 0.2f),
              "abs(-6) / 30: " + std::to_string(volume));
    CHECK_MSG(!Sounds::ImpactVolume(crate, 2.0, volume), "at 2 it is not above 2");
    Sounds::Impact player = crate;
    player.absolute = false;
    player.hitAbove = 3.5;
    player.volumeDivisor = 18.0;
    CHECK_MSG(!Sounds::ImpactVolume(player, -9.0, volume), "the player's is not taken absolute");
    CHECK_MSG(Sounds::ImpactVolume(player, 9.0, volume) && ::test::nearly(static_cast<float>(volume), 0.5f),
              "9 / 18: " + std::to_string(volume));
}

// The port's real file, and the decode it carries.
void ThePortsOwnTableIsTheOriginals() {
    Sounds::Rules rules;
    std::string error;
    const std::string path = kPortData + "/sounds.json";
    CHECK_MSG(Sounds::LoadRules(path, rules, error), path + ": " + error);
    if (rules.hooks.empty()) return;

    // The original's AudioManager has upwards of sixty hooks; a table that lost
    // most of them would still read.
    CHECK_MSG(rules.hooks.size() >= 55,
              "expected the whole AudioManager, got " + std::to_string(rules.hooks.size()) + " hooks");

    const auto plays = [&rules](const char* hook, const char* file) {
        const Sounds::Hook* found = rules.FindHook(hook);
        CHECK_MSG(found != nullptr, std::string(hook) + " should be in the table");
        if (found == nullptr) return;
        CHECK_MSG(!found->files.empty() && found->files[0] == file,
                  std::string(hook) + " plays " + file + ", not " +
                      (found->files.empty() ? std::string("nothing") : found->files[0]));
    };

    // The plain ones, which are most of them.
    plays("playPortalCreatedSound", "portal_created.mp3");
    plays("playPortalLaunchSound", "portal_launch.mp3");
    plays("playTeleportSound", "teleport.mp3");
    plays("playDeathSound", "dark_whoosh_17.mp3");
    plays("playDemolitionSound", "demolition.mp3");
    plays("playElevatorSound", "elevator.mp3");
    plays("playDoorOpenSound", "door_open.mp3");
    plays("playReflectSound", "projectile_reflect.mp3");

    // And the ones nobody would guess. These are the whole reason this test
    // exists: each was read off the bytecode and each looks like a mistake.
    plays("playCrystalVanishSound", "spike_hit.mp3");   // entry 25, for a crystal
    plays("playDoorUnlockSound", "crystal_gather_G.mp3"); // a crystal, for a door
    plays("playStoneAppearSound", "portal_created.mp3");
    plays("playBeholderHitSound", "demolition.mp3");
    plays("playLevelFinishedSound", "teleport.mp3");
    plays("playEnemyShootExplodeSound", "portal_fail.mp3");
    // The menu's button is the ONLY thing in the game that plays button.mp3.
    plays("getButtonSoundName", "button.mp3");
    // Every one of the level's own buttons is a teleport.
    plays("getRetryButtonSound", "teleport.mp3");

    // The door unlocks at half speed, which is what makes it a door.
    const Sounds::Hook* unlock = rules.FindHook("playDoorUnlockSound");
    if (unlock != nullptr) CHECK_NEAR(static_cast<float>(unlock->speed), 0.5f);

    // playVictorySound sets the speed BACK to 1.0, because the light and
    // roundabout hooks leave that same file at 0.6.
    const Sounds::Hook* victory = rules.FindHook("playVictorySound");
    const Sounds::Hook* lightOn = rules.FindHook("playLightOnSound");
    const Sounds::Hook* roundabout = rules.FindHook("playRoundaboutSound");
    CHECK_MSG(victory != nullptr && lightOn != nullptr && roundabout != nullptr, "the sweep hooks should read");
    if (victory != nullptr && lightOn != nullptr && roundabout != nullptr) {
        CHECK_MSG(victory->files.size() == 1 && victory->files[0] == "magical_sweep_08.mp3",
                  "victory is the sweep");
        CHECK_NEAR(static_cast<float>(victory->speed), 1.0f);
        CHECK_NEAR(static_cast<float>(lightOn->speed), 0.6f);
        // playLightOnSound and playRoundaboutSound are the same bytes.
        CHECK_MSG(lightOn->files == roundabout->files && lightOn->both == roundabout->both &&
                      ::test::nearly(static_cast<float>(lightOn->speed), static_cast<float>(roundabout->speed)),
                  "light-on and roundabout are byte for byte the same hook");
    }

    // ONE EXPLOSION, CHOSEN BY ITS ARGUMENT. playExplosionSound(bool huge):
    // #3 CpyVtoR4 v-1[huge], #4 JLowZ -> 33 - explosion_huge alone on its own
    // 150 ms timer, or explosion_small alone on another. This pinned "both
    // samples together" until planning step 142, which was the misdecode.
    CHECK_MSG(rules.FindHook("playExplosionSound") == nullptr,
              "no hook plays both explosions: the function is its two calls");
    const Sounds::Hook* huge = rules.FindHook("playExplosionSound(true)");
    const Sounds::Hook* small = rules.FindHook("playExplosionSound(false)");
    CHECK_MSG(huge != nullptr && small != nullptr, "both arms of playExplosionSound should read");
    if (huge != nullptr && small != nullptr) {
        CHECK_MSG(huge->files == std::vector<std::string>{"explosion_huge.mp3"} && !huge->both,
                  "true is explosion_huge, alone");
        CHECK_MSG(small->files == std::vector<std::string>{"explosion_small.mp3"} && !small->both,
                  "false is explosion_small, alone");
        CHECK_MSG(huge->timer == "hugeExplosionSoundTimer" && small->timer == "smallExplosionSoundTimer",
                  "each arm on its own timer");
        CHECK_NEAR(static_cast<float>(huge->minIntervalMs), 150.0f);
        CHECK_NEAR(static_cast<float>(small->minIntervalMs), 150.0f);
    }

    // One of two at random, and the shared timer.
    const Sounds::Hook* crystal = rules.FindHook("playCrystalPickSound");
    const Sounds::Hook* alert = rules.FindHook("playCrystalTempAlertSound");
    CHECK_MSG(crystal != nullptr && alert != nullptr, "the crystal hooks should read");
    if (crystal != nullptr && alert != nullptr) {
        CHECK_MSG(crystal->files.size() == 2 && !crystal->both, "a crystal is one of two at random");
        CHECK_NEAR(static_cast<float>(crystal->volume), 0.8f);
        // The shared timer: the two crystal sounds hold EACH OTHER off, which
        // is one Timer in the original, not one per hook - each against its
        // own number (CMPIu v1, v50 and CMPIu v1, v100).
        CHECK_MSG(!crystal->timer.empty() && crystal->timer == alert->timer,
                  "the pick and the alert share one timer");
        CHECK_NEAR(static_cast<float>(crystal->minIntervalMs), 50.0f);
        CHECK_NEAR(static_cast<float>(alert->minIntervalMs), 100.0f);
    }

    // The wood drag is the hook that proved the whole table had to be keyed on
    // what reaches PlaySample: it draws a speed for entry 2 and plays entry 22,
    // whose own speed nothing sets.
    const Sounds::Hook* drag = rules.FindHook("playWoodDragSound");
    CHECK_MSG(drag != nullptr, "playWoodDragSound should read");
    if (drag != nullptr) {
        CHECK_MSG(drag->files.size() == 1 && drag->files[0] == "wood_drag.mp3",
                  "the drag plays wood_drag, not the portal_fail it only sets the speed on");
        CHECK_MSG(!drag->speedIsRandom && ::test::nearly(static_cast<float>(drag->speed), 1.0f),
                  "and wood_drag is heard at 1.0: the drawn speed is portal_fail's");
        CHECK_MSG(drag->timer == "woodDragSoundTimer", "on woodDragSoundTimer");
        CHECK_NEAR(static_cast<float>(drag->minIntervalMs), 200.0f);
    }

    // The rest of the audit's section 5, each read off its hook's body.
    const auto speedOf = [&rules](const char* hook, float speed) {
        const Sounds::Hook* found = rules.FindHook(hook);
        CHECK_MSG(found != nullptr && !found->speedIsRandom && !found->speedFromDoorStride &&
                      ::test::nearly(static_cast<float>(found->speed), speed),
                  std::string(hook) + " at speed " + std::to_string(speed));
    };
    speedOf("playKeyPickSound", 0.5f);
    speedOf("playKeyUnlockSound", 0.5f);
    speedOf("playLightSwitchSound", 0.5f);
    speedOf("playGhostDeathSound", 0.5f);
    speedOf("playFireExtinguishSound", 0.5f);
    const Sounds::Hook* close = rules.FindHook("playDoorCloseSound");
    CHECK_MSG(close != nullptr && close->speedFromDoorStride, "a door closes at 3000 / its stride, as it opens");
    const auto argumentOf = [&rules](const char* hook, const char* timer, float intervalMs) {
        const Sounds::Hook* found = rules.FindHook(hook);
        CHECK_MSG(found != nullptr && found->volumeFromArgument, std::string(hook) + " takes its caller's volume");
        if (found == nullptr) return;
        CHECK_MSG(found->timer == timer && ::test::nearly(static_cast<float>(found->minIntervalMs), intervalMs),
                  std::string(hook) + " on " + timer + " at " + std::to_string(intervalMs) + " ms");
    };
    argumentOf("playFallSound", "charHitSoundTimer", 200.0f);
    argumentOf("playRandomWoodSound", "woodSoundTimer", 150.0f);
    argumentOf("playStoneHitSound", "", 0.0f);
    for (const char* drawnSpeed : {"playRandomWoodSound", "playStoneHitSound"}) {
        const Sounds::Hook* found = rules.FindHook(drawnSpeed);
        CHECK_MSG(found != nullptr && found->speedIsRandom && ::test::nearly(static_cast<float>(found->speedFrom), 0.8f) &&
                      ::test::nearly(static_cast<float>(found->speedTo), 1.0f),
                  std::string(drawnSpeed) + " draws randF(0.2) + 0.8");
    }
    const Sounds::Hook* fireball = rules.FindHook("playFireballSound");
    CHECK_MSG(fireball != nullptr && fireball->timer == "fireballSoundTimer" &&
                  ::test::nearly(static_cast<float>(fireball->minIntervalMs), 100.0f),
              "a fireball on its 100 ms timer");

    // The five tracks, at the volumes the original starts them.
    const Sounds::Track* menu = rules.FindTrack("menu");
    const Sounds::Track* game = rules.FindTrack("game");
    const Sounds::Track* boss = rules.FindTrack("boss");
    CHECK_MSG(menu != nullptr && game != nullptr && boss != nullptr, "menu, game and boss music should read");
    if (menu != nullptr && game != nullptr && boss != nullptr) {
        CHECK_MSG(menu->file == "warlords_loop_a.mp3", "the menu is warlords_loop_a");
        CHECK_MSG(game->file == "finding_wonderland_60_loop.mp3", "a level is finding_wonderland");
        CHECK_MSG(boss->file == "running_with_wolves_loop_b.mp3", "a boss is running_with_wolves");
        CHECK_NEAR(static_cast<float>(menu->volume), 1.0f);
        CHECK_NEAR(static_cast<float>(game->volume), 0.7f);
        CHECK_NEAR(static_cast<float>(boss->volume), 0.6f);
        CHECK_MSG(menu->loop && game->loop && boss->loop, "all three loop");
    }
    CHECK_MSG(rules.music.size() == 5, "the original names five tracks, got " + std::to_string(rules.music.size()));

    // The events the layer actually latches, each resolving to a hook.
    const auto playsFor = [&rules](const char* event, const char* hook) {
        const Sounds::Hook* found = rules.ForEvent(event);
        CHECK_MSG(found != nullptr, std::string(event) + " should name a hook");
        if (found != nullptr) CHECK_MSG(found->name == hook, std::string(event) + " plays " + found->name);
    };
    playsFor("portal_placed", "playPortalCreatedSound");
    playsFor("shot_fired", "playPortalLaunchSound");
    playsFor("shot_failed", "playPortalFailedSound");
    playsFor("traversal", "playTeleportSound");
    playsFor("crystal_collected", "playCrystalPickSound");
    playsFor("player_died", "playDeathSound");
    // THE FINISH IS THE DOOR'S SOUND, not the medal's, and this pinned the wrong
    // one. GameStateController::checkGameEnd calls levelFinishedEffect - which
    // ends in playFinalDoorSound - then hides the character, and only once
    // gameWonDelay (1400 ms) has passed does it raise the finish layer and play
    // playVictorySound. The cue this used to name, playLevelFinishedSound, is
    // defined in AudioManager and called by NOTHING in the binary: a grep for its
    // CALLINTF finds the definition and no call site. sounds.json keeps it listed
    // as a decoded cue with no moment rather than deleting it.
    playsFor("level_finished", "playFinalDoorSound");
    // Dying is two moments as finishing is: the fall's own cue at the moment it
    // happens, and the lost screen's 1400 ms later. An hp death has no cue of
    // its own at all - checkGameLost plays one only on the bounds branch.
    playsFor("player_fell", "playDieByFallSound");
    playsFor("player_died", "playDeathSound");
    playsFor("medal_shown", "playVictorySound");
    playsFor("wall_broken", "playDemolitionSound");
    playsFor("menu_button", "getButtonSoundName");
    playsFor("level_button", "getRetryButtonSound");
    // The dashboard's rows: a tap on an open level's row, and on a locked one's.
    playsFor("achievement_pick", "playAchievementPickSound");
    playsFor("achievement_denied", "playAchievementPickDeniedSound");

    // PLANNING STEP 142, the sound pass: each event at the hook its decoded call
    // site plays (sounds.json carries every citation).
    playsFor("crystal_alert", "playCrystalTempAlertSound");
    playsFor("key_picked", "playKeyPickSound");
    playsFor("door_unlocked", "playDoorUnlockSound");
    playsFor("key_unlocked", "playKeyUnlockSound");
    playsFor("keyhole_vanished", "playDemolitionSound");
    playsFor("player_landed", "playFallSound");
    playsFor("minion_landed", "playFallSound");
    playsFor("minion_fell", "playMinionFallSound");
    playsFor("crate_knocked", "playRandomWoodSound");
    playsFor("crate_dragged", "playWoodDragSound");
    playsFor("stone_hit", "playStoneHitSound");
    playsFor("stone_broken", "playStoneHitSound");
    playsFor("fireball_spat", "playFireballSound");
    playsFor("fireball_burst", "playExplosionSound(false)");
    playsFor("fireball_on_shock_agent", "playExplosionSound(true)");
    playsFor("bomb_blast", "playExplosionSound(true)");
    playsFor("agent_killed", "playExplosionSound(true)");
    playsFor("burned", "playBurnSound");
    playsFor("fire_diamond_drained", "playFireExtinguishSound");
    playsFor("shock_picked", "playShockDiamondPickSound");
    playsFor("shock_struck", "playShockStrikeSound");
    playsFor("light_on", "playLightOnSound");
    playsFor("light_off", "playLightOffSound");
    playsFor("light_switch", "playLightSwitchSound");
    playsFor("boss_appeared", "playBeholderAppearSound");
    playsFor("boss_struck", "playBeholderHitSound");
    playsFor("boss_hurt", "playBeholderDamageSound");
    playsFor("boss_spike_hit", "playBeholderSpikeHitSound");
    playsFor("boss_rock_dropped", "playStoneAppearSound");
    playsFor("boss_explosion", "playExplosionSound(false)");
    playsFor("boss_dead", "playExplosionSound(true)");
    playsFor("dragon_appeared", "playDragonAppearSound");
    playsFor("dragon_took_off", "playDragonTakeOffSound");
    playsFor("claw_crushed", "playDemolitionSound");
    playsFor("ghost_appeared", "playGhostAppearSound");
    playsFor("ghost_item", "playGhostItemAppearSound");
    playsFor("ghost_hurt", "playGhostDamageSound");
    playsFor("ghost_dead", "playGhostDeathSound");
    playsFor("ghost_burst", "playExplosionSound(true)");
    playsFor("enemy_shot", "playEnemyShootSound");
    playsFor("enemy_shot_ended", "playEnemyShootExplodeSound");
    // A knight's sight (planning step 158, G1a): the spotted sound with the exclamation, the question sound with the question.
    playsFor("enemy_spotted", "playEnemySpottedSound");
    playsFor("minion_question", "playMinionQuestionMarkSound");
    playsFor("dark_dragon_appeared", "playDarkDragonAppearSound");
    playsFor("dark_dragon_mouth", "playDarkDragonOpenMouthSound");
    playsFor("dark_dragon_hurt", "playDarkDragonDamageSound");
    playsFor("dark_dragon_item", "playGhostItemAppearSound");
    playsFor("dark_dragon_dead", "playDarkDragonDeathSound");
    playsFor("dark_dragon_burst", "playExplosionSound(true)");
    // And the ones the original has no sound for, which the port had given one:
    // a launcher's throw calls no audio function; a beholder rock's break is
    // its landing's stone_hit; the beholder's button moves in silence.
    for (const char* silent : {"stone_thrown", "boss_rock_broken", "boss_button_raised"}) {
        CHECK_MSG(rules.events.count(silent) == 1 && rules.ForEvent(silent) == nullptr,
                  std::string(silent) + " is named, and silent");
    }

    // The contacts' own gates and volumes (sounds.json impacts).
    const Sounds::Impacts& impacts = rules.impacts;
    CHECK_MSG(impacts.player.event == "player_landed" && impacts.player.hitAbove == 3.5 &&
                  impacts.player.volumeDivisor == 18.0 && !impacts.player.absolute && impacts.player.spares.empty(),
              "the player's thump: above 3.5, hit / 18, and no crate test");
    CHECK_MSG(impacts.minion.event == "minion_landed" && impacts.minion.hitAbove == 3.5 &&
                  impacts.minion.volumeDivisor == 18.0 &&
                  impacts.minion.spares == std::vector<std::string>{"enemy_killer"},
              "a minion's: the same, and not on the killer floor");
    CHECK_MSG(impacts.stone.event == "stone_hit" && impacts.stone.hitAbove == 2.0 &&
                  impacts.stone.volumeDivisor == 3.0 && impacts.stone.destroyierEvent == "stone_broken" &&
                  impacts.stone.destroyierVolume == 1.0,
              "a stone's: above 2 at hit / 3, and 1 on a destroyier");
    CHECK_MSG(impacts.crate.event == "crate_knocked" && impacts.crate.characterEvent == "crate_dragged" &&
                  impacts.crate.hitAbove == 2.0 && impacts.crate.volumeDivisor == 30.0 && impacts.crate.absolute &&
                  impacts.crate.entities.size() == 12,
              "a crate's: |hit| above 2 at |hit| / 30, the drag against the character, six callbacks' twelve names");
    const Sounds::Cues& cues = rules.cues;
    CHECK_MSG(cues.beholderAppearMarginXPx == 48.0 && cues.beholderAppearMarginYPx == 32.0 &&
                  cues.dragonAppearAfterMs == 2000.0 && cues.bossExplosionStrideMs == 400.0 &&
                  cues.lightSwitchAfterMs == 800.0,
              "the cues: scale(48, 32), 2000 ms, 400 ms, 800 ms");
}

// The one case that opens the original's own files.
void EverySoundNamedIsReallyThere() {
    Sounds::Rules rules;
    std::string error;
    if (!Sounds::LoadRules(kPortData + "/sounds.json", rules, error)) return; // said so above

    const std::string directory = Sounds::Directory(kOriginal);
    std::error_code ec;
    if (!std::filesystem::is_directory(directory, ec)) {
        std::printf("  no sound files at %s - the part that opens them is skipped\n", directory.c_str());
        return;
    }

    // Three silent-missing-art bugs in two days went unnoticed because nothing
    // asserted the files were there. A sound that is not there is quieter
    // still: there is not even a blank square to see.
    int checked = 0;
    std::string missing;
    const auto exists = [&](const std::string& file, const std::string& owner) {
        ++checked;
        std::error_code fileEc;
        if (std::filesystem::is_regular_file(directory + "/" + file, fileEc)) return;
        if (missing.empty()) missing = file + " (" + owner + ")";
    };
    for (const auto& [name, hook] : rules.hooks) {
        for (const std::string& file : hook.files) exists(file, name);
    }
    for (const auto& [name, track] : rules.music) exists(track.file, name);

    std::printf("  %d sound file(s) named, in %s\n", checked, directory.c_str());
    CHECK_MSG(checked > 50, "expected the whole table to name files, got " + std::to_string(checked));
    CHECK_MSG(missing.empty(), "named but not in soundfx/: " + missing);
}

void runTests() {
    TheReaderRefusesWhatWouldBeSilent();
    TheSpeedShapesRead();
    TheArgumentVolumeAndTheImpactsRead();
    ThePortsOwnTableIsTheOriginals();
    EverySoundNamedIsReallyThere();
}

} // namespace

MP_TEST_MAIN("test_mp_sounds", 45)
