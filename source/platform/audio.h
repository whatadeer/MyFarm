#pragma once

#include "core/music.h"

namespace platform {

// One-shot sound effects (Sprout Sorry pack WAVs, pre-trimmed to raw mono
// PCM16 @22050Hz in romfs:/sfx/ by tools/prep_assets.py). Playback is
// fire-and-forget across a small rotating pool of ndsp channels.
enum class Sfx {
    Till = 0,
    Dig,
    Chop,
    Mine,
    Demolish,
    Plant,
    Place,
    Harvest,
    LevelUp,
    Deny,
    Ui,
    // The rest of the pack's WAVs (see tools/prep_assets.py SOUNDS).
    Bite,    // fish on the line - attention ring
    Splash,  // reel-in catch / entering water
    Eat,     // drinking a potion
    Hurt,    // player takes damage
    Hit,     // striking an enemy
    Kill,    // enemy defeated
    Bonk,    // hammer-bonking a rock
    Mail,    // checking the mailbox
    Chime,   // furniture variant cycle
    Tame,    // taming a wild animal
    Descend, // riding the mineshaft down
    Count,
};

// ndspInit() needs the console's DSP firmware dump (sdmc:/3ds/dspfirm.cdc,
// created by running the DSP1 homebrew once - most CFW setups have it).
// If it's missing, audioInit() quietly leaves audio disabled and every
// playSfx() becomes a no-op rather than an error the player must care
// about. Returns whether sound is actually available.
bool audioInit();
void audioShutdown();
void playSfx(Sfx sfx);

// Procedurally-generated background music (see core/music.h +
// core/chip_synth.h for the composer/synth) - an endless, non-repeating
// bar-by-bar chiptune stream rather than a looped track, mood-selectable so
// day/night/etc. can each feel different. All no-ops if audio isn't
// available (see audioInit()).
//
// setMusicMood() starts music on first call; later calls change the mood
// at the *next bar boundary* rather than cutting off mid-phrase, so calling
// it every frame while e.g. darkness ramps up is harmless - it's a target,
// not an instant switch.
void setMusicMood(core::Mood mood);
void stopMusic();
// Refills whichever streaming buffer(s) ndsp has finished playing. Call
// once per frame from the main loop; cheap when nothing is due yet.
void updateMusic();

} // namespace platform
