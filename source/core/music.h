#pragma once

#include <cstdint>
#include <vector>

namespace core {

// Procedural chiptune composer. A first-order Markov chain walks scale
// degrees for the melody while a Euclidean rhythm drives percussion, both
// parameterized per Mood so the same machinery yields three different
// personalities. Everything here is a pure function of (mood, seed,
// barIndex) - no randomness source outside Rng, no hidden state - so it's
// host-testable and platform::audio can regenerate any bar on demand
// without having to store it.
//
// chipsynth.h turns the Bar this produces into actual PCM; this file only
// ever deals in scale degrees, samples-as-timing-units, and volumes.
enum class Mood : uint8_t { Calm = 0, Happy = 1, Spooky = 2, Count = 3 };

enum class Wave : uint8_t { Square, Triangle, Noise };

// One instrument note or percussion hit. `startSample`/`lengthSamples` are
// offsets within the Bar they belong to, already converted from
// steps/tempo to samples so chipsynth never needs to know about timing.
struct Note {
    uint32_t startSample = 0;
    uint32_t lengthSamples = 0;
    float freqHz = 0.0f; // ignored for Wave::Noise
    Wave wave = Wave::Square;
    float volume = 0.0f; // 0..1, pre-mix
};

struct Bar {
    uint32_t totalSamples = 0;
    std::vector<Note> notes; // melody + bass + percussion, unsorted
};

// Small deterministic PRNG (xorshift32) - not cryptographic, just cheap and
// seedable. Zero seeds are nudged off zero (xorshift's one fixed point).
struct Rng {
    uint32_t state;
    explicit Rng(uint32_t seed) : state(seed ? seed : 0x9E3779B9u) {}
    uint32_t next();
    float nextFloat();     // [0,1)
    int nextInt(int count); // [0,count) - count must be > 0
};

// Spreads `hits` pulses as evenly as possible across `steps` slots, via a
// Bresenham/accumulator scan - the same "maximally even" output class as
// Bjorklund's recursive Euclidean-rhythm algorithm, in a few lines instead
// of a recursive bucket merge. hits<=0 -> all false; hits>=steps -> all true.
std::vector<bool> euclideanRhythm(int steps, int hits);

// Renders one bar (length depends on the mood's tempo, roughly 2-4s) of
// melody + bass + percussion. `barIndex` seeds bar-to-bar variation - call
// with 0,1,2,... for a continuously-evolving piece. The same
// (mood, seed, barIndex, sampleRate) always reproduces the same bar.
Bar generateBar(Mood mood, uint32_t seed, uint32_t barIndex, uint32_t sampleRate);

} // namespace core
