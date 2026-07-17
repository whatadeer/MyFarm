#include "core/music.h"

#include <algorithm>
#include <cmath>

namespace core {

uint32_t Rng::next() {
    uint32_t x = state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state = x;
    return x;
}

float Rng::nextFloat() {
    // Top 24 bits -> [0,1); avoids the low bits of xorshift32, which are
    // weaker than the high ones.
    return static_cast<float>(next() >> 8) / static_cast<float>(1u << 24);
}

int Rng::nextInt(int count) {
    return static_cast<int>(next() % static_cast<uint32_t>(count));
}

std::vector<bool> euclideanRhythm(int steps, int hits) {
    std::vector<bool> pattern(static_cast<size_t>(steps > 0 ? steps : 0), false);
    if (steps <= 0 || hits <= 0) return pattern;
    if (hits >= steps) {
        std::fill(pattern.begin(), pattern.end(), true);
        return pattern;
    }
    int bucket = 0;
    for (int i = 0; i < steps; i++) {
        bucket += hits;
        if (bucket >= steps) {
            bucket -= steps;
            pattern[static_cast<size_t>(i)] = true;
        }
    }
    return pattern;
}

namespace {

constexpr int kStepsPerBar = 16;

constexpr int8_t kMajorPentatonic[] = {0, 2, 4, 7, 9};
constexpr int8_t kMajorScale[] = {0, 2, 4, 5, 7, 9, 11};
// Phrygian's flat 2nd (one semitone above the root) is what gives it that
// "spooky" cadence - a half-step is far more unsettling than the minor
// scale's whole-step 2nd.
constexpr int8_t kPhrygianScale[] = {0, 1, 3, 5, 7, 8, 10};

struct MoodProfile {
    const int8_t* scale;
    int scaleLen;
    int rootMidi;
    float bpm;
    float density;   // probability a given melody step sounds a note
    int octaveSpan;  // melody register width, in octaves
    Wave melodyWave;
    Wave bassWave;
    float melodyVolume;
    float bassVolume;
    float percVolume;
    int percHits;     // euclidean hits per kStepsPerBar-step bar
    bool droneBass;    // one sustained bass note for the whole bar
    bool spookySting;  // occasional dissonant high flourish
};

const MoodProfile& profileFor(Mood mood) {
    static const MoodProfile kProfiles[3] = {
        // Calm: major pentatonic, slow, sparse, everything triangle-soft.
        {kMajorPentatonic, 5, 57, 76.0f, 0.5f, 1, Wave::Triangle, Wave::Triangle, 0.35f, 0.22f,
         0.12f, 3, false, false},
        // Happy: major scale, brisk, dense, square-wave bounce.
        {kMajorScale, 7, 64, 128.0f, 0.75f, 2, Wave::Square, Wave::Square, 0.30f, 0.20f, 0.16f, 7,
         false, false},
        // Spooky: phrygian, slow and sparse, low sustained drone bass,
        // occasional dissonant sting.
        {kPhrygianScale, 7, 50, 58.0f, 0.3f, 1, Wave::Square, Wave::Triangle, 0.22f, 0.18f, 0.10f,
         3, true, true},
    };
    return kProfiles[static_cast<int>(mood)];
}

float degreeToFreq(const MoodProfile& mp, int degree) {
    int semis = mp.scale[degree % mp.scaleLen] + 12 * (degree / mp.scaleLen);
    int midi = mp.rootMidi + semis;
    return 440.0f * std::pow(2.0f, static_cast<float>(midi - 69) / 12.0f);
}

// One weighted random step of the melody's scale-degree Markov walk. The
// transition weights (not the destination) are all that depend on `mood`,
// which is what makes this a proper per-mood Markov chain rather than a
// single shared random walk. Steps that would leave [0,maxDeg] reflect back
// in rather than clamping flat, so the melody doesn't get stuck riding the
// register ceiling/floor.
int walkDegree(Mood mood, Rng& rng, int current, int maxDeg) {
    static const int8_t kCalmDeltas[] = {-2, -1, 0, 1, 2};
    static const int kCalmWeights[] = {1, 3, 2, 3, 1};
    static const int8_t kHappyDeltas[] = {-3, -2, -1, 0, 1, 2, 3, 4};
    static const int kHappyWeights[] = {1, 2, 2, 1, 2, 3, 2, 2};
    static const int8_t kSpookyDeltas[] = {-4, -2, -1, 0, 1, 2, 4};
    static const int kSpookyWeights[] = {1, 2, 3, 3, 3, 2, 1};

    const int8_t* deltas = kCalmDeltas;
    const int* weights = kCalmWeights;
    int n = 5;
    if (mood == Mood::Happy) {
        deltas = kHappyDeltas;
        weights = kHappyWeights;
        n = 8;
    } else if (mood == Mood::Spooky) {
        deltas = kSpookyDeltas;
        weights = kSpookyWeights;
        n = 7;
    }

    int totalWeight = 0;
    for (int i = 0; i < n; i++) totalWeight += weights[i];
    int roll = rng.nextInt(totalWeight);
    int delta = deltas[n - 1];
    for (int i = 0; i < n; i++) {
        if (roll < weights[i]) {
            delta = deltas[i];
            break;
        }
        roll -= weights[i];
    }

    int next = current + delta;
    if (next < 0 || next > maxDeg) next = current - delta; // reflect off the edge
    if (next < 0) next = 0;
    if (next > maxDeg) next = maxDeg;
    return next;
}

} // namespace

Bar generateBar(Mood mood, uint32_t seed, uint32_t barIndex, uint32_t sampleRate) {
    const MoodProfile& mp = profileFor(mood);
    Rng rng(seed ^ (barIndex * 0x2545F491u) ^ (static_cast<uint32_t>(mood) * 0x9E3779B9u + 1u));

    float stepSeconds = 60.0f / mp.bpm / 4.0f; // 16th-note grid over a 4/4 bar
    uint32_t stepSamples = static_cast<uint32_t>(stepSeconds * static_cast<float>(sampleRate));

    Bar bar;
    bar.totalSamples = stepSamples * kStepsPerBar;

    int maxDeg = mp.scaleLen * mp.octaveSpan - 1;
    int degree = maxDeg / 2;

    // Melody: a Markov walk over scale degrees, one weighted step per
    // active grid slot. Sustained notes (2 steps) skip the walk forward so
    // they aren't immediately retriggered mid-sustain.
    for (int step = 0; step < kStepsPerBar; step++) {
        if (rng.nextFloat() >= mp.density) continue;
        degree = walkDegree(mood, rng, degree, maxDeg);
        int sustainSteps = (rng.nextFloat() < (mood == Mood::Happy ? 0.25f : 0.55f)) ? 2 : 1;
        if (step + sustainSteps > kStepsPerBar) sustainSteps = kStepsPerBar - step;
        uint32_t start = static_cast<uint32_t>(step) * stepSamples;
        uint32_t len = stepSamples * static_cast<uint32_t>(sustainSteps);
        bar.notes.push_back(Note{start, len, degreeToFreq(mp, degree), mp.melodyWave, mp.melodyVolume});
        step += sustainSteps - 1;
    }

    // Bass: a sustained drone for Spooky (dread needs continuity), otherwise
    // one detached note per beat alternating the root with a higher scale
    // step for a walking feel, an octave below the melody's register.
    if (mp.droneBass) {
        bar.notes.push_back(Note{0, bar.totalSamples, degreeToFreq(mp, 0) * 0.5f, mp.bassWave, mp.bassVolume});
    } else {
        int upperDeg = mp.scaleLen > 4 ? 4 : mp.scaleLen - 1;
        for (int beat = 0; beat < kStepsPerBar / 4; beat++) {
            int bassDeg = (beat % 2 == 0) ? 0 : upperDeg;
            uint32_t start = static_cast<uint32_t>(beat * 4) * stepSamples;
            uint32_t len = stepSamples * 3; // slightly detached, not legato
            bar.notes.push_back(Note{start, len, degreeToFreq(mp, bassDeg) * 0.5f, mp.bassWave, mp.bassVolume});
        }
    }

    // Percussion: Euclidean-distributed noise hits.
    std::vector<bool> pattern = euclideanRhythm(kStepsPerBar, mp.percHits);
    for (int step = 0; step < kStepsPerBar; step++) {
        if (!pattern[static_cast<size_t>(step)]) continue;
        uint32_t start = static_cast<uint32_t>(step) * stepSamples;
        bar.notes.push_back(Note{start, stepSamples / 2, 0.0f, Wave::Noise, mp.percVolume});
    }

    // Spooky flourish: an occasional dissonant high sting (root + tritone,
    // up an octave), placed clear of the bar edges so its envelope always
    // fits inside the bar.
    if (mp.spookySting && rng.nextFloat() < 0.2f) {
        int stingStep = 2 + rng.nextInt(kStepsPerBar - 4);
        float freq = degreeToFreq(mp, 0) * std::pow(2.0f, 18.0f / 12.0f);
        uint32_t start = static_cast<uint32_t>(stingStep) * stepSamples;
        bar.notes.push_back(Note{start, stepSamples, freq, Wave::Square, mp.melodyVolume * 0.7f});
    }

    return bar;
}

} // namespace core
