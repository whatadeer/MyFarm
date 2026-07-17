// Host-native tests for source/core/music.h: the Euclidean rhythm helper,
// the Rng, and the per-mood bar generator.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "core/music.h"
#include "minitest.h"

using namespace core;

static void test_euclidean_edge_cases() {
    // hits <= 0 -> nothing.
    auto none = euclideanRhythm(8, 0);
    CHECK(none.size() == 8);
    for (bool b : none) CHECK(!b);

    auto negative = euclideanRhythm(8, -3);
    for (bool b : negative) CHECK(!b);

    // hits >= steps -> everything.
    auto full = euclideanRhythm(8, 8);
    for (bool b : full) CHECK(b);
    auto overfull = euclideanRhythm(8, 99);
    for (bool b : overfull) CHECK(b);

    // steps <= 0 -> empty, doesn't crash.
    CHECK(euclideanRhythm(0, 3).empty());
}

static void test_euclidean_hit_count_and_spread() {
    // Exactly `hits` true slots, and no gap between consecutive hits (with
    // wraparound) more than double the smallest gap - "evenly spread", not
    // clumped at one end.
    for (int hits = 1; hits < 8; hits++) {
        auto pattern = euclideanRhythm(8, hits);
        int count = 0;
        std::vector<int> positions;
        for (int i = 0; i < 8; i++) {
            if (pattern[static_cast<size_t>(i)]) {
                count++;
                positions.push_back(i);
            }
        }
        CHECK(count == hits);

        if (positions.size() >= 2) {
            int minGap = 999, maxGap = 0;
            for (size_t i = 0; i < positions.size(); i++) {
                int next = positions[(i + 1) % positions.size()];
                int gap = next - positions[i];
                if (gap <= 0) gap += 8;
                minGap = std::min(minGap, gap);
                maxGap = std::max(maxGap, gap);
            }
            CHECK(maxGap <= minGap * 2);
        }
    }
}

static void test_euclidean_deterministic() {
    CHECK(euclideanRhythm(16, 5) == euclideanRhythm(16, 5));
}

static void test_rng_deterministic_and_ranged() {
    Rng a(1234);
    Rng b(1234);
    for (int i = 0; i < 50; i++) {
        CHECK(a.next() == b.next());
    }

    Rng r(777);
    bool sawNonZeroFloat = false;
    for (int i = 0; i < 200; i++) {
        float f = r.nextFloat();
        CHECK(f >= 0.0f && f < 1.0f);
        if (f > 0.0f) sawNonZeroFloat = true;

        int n = r.nextInt(7);
        CHECK(n >= 0 && n < 7);
    }
    CHECK(sawNonZeroFloat);

    // Seed 0 is xorshift's fixed point - must be nudged off it rather than
    // producing an all-zero stream.
    Rng zero(0);
    CHECK(zero.next() != 0u);
}

static bool notesEqual(const Note& a, const Note& b) {
    return a.startSample == b.startSample && a.lengthSamples == b.lengthSamples &&
           a.freqHz == b.freqHz && a.wave == b.wave && a.volume == b.volume;
}

static bool barsEqual(const Bar& a, const Bar& b) {
    if (a.totalSamples != b.totalSamples) return false;
    if (a.notes.size() != b.notes.size()) return false;
    for (size_t i = 0; i < a.notes.size(); i++) {
        if (!notesEqual(a.notes[i], b.notes[i])) return false;
    }
    return true;
}

static void test_generate_bar_deterministic() {
    Bar a = generateBar(Mood::Happy, 42, 3, 22050);
    Bar b = generateBar(Mood::Happy, 42, 3, 22050);
    CHECK(barsEqual(a, b));
}

static void test_generate_bar_varies_across_bars_and_seeds() {
    Bar bar0 = generateBar(Mood::Calm, 1, 0, 22050);
    Bar bar1 = generateBar(Mood::Calm, 1, 1, 22050);
    CHECK(!barsEqual(bar0, bar1));

    Bar seedA = generateBar(Mood::Spooky, 1, 0, 22050);
    Bar seedB = generateBar(Mood::Spooky, 2, 0, 22050);
    CHECK(!barsEqual(seedA, seedB));
}

static void test_generate_bar_notes_stay_in_bounds() {
    for (int m = 0; m < static_cast<int>(Mood::Count); m++) {
        Mood mood = static_cast<Mood>(m);
        for (uint32_t barIndex = 0; barIndex < 6; barIndex++) {
            Bar bar = generateBar(mood, 999, barIndex, 22050);
            CHECK(bar.totalSamples > 0);
            CHECK(!bar.notes.empty());
            for (const Note& n : bar.notes) {
                CHECK(n.lengthSamples > 0);
                CHECK(n.startSample + n.lengthSamples <= bar.totalSamples);
                CHECK(n.volume > 0.0f && n.volume <= 1.0f);
                if (n.wave != Wave::Noise) {
                    CHECK(n.freqHz > 0.0f);
                }
            }
        }
    }
}

static void test_mood_tempo_and_density_differ() {
    // Spooky is the slowest mood -> its bar (same step count, same sample
    // rate) spans more samples than Happy's, the fastest.
    Bar spooky = generateBar(Mood::Spooky, 5, 0, 22050);
    Bar happy = generateBar(Mood::Happy, 5, 0, 22050);
    CHECK(spooky.totalSamples > happy.totalSamples);

    // Happy is denser than Spooky - averaged over several bars (a single
    // bar's random note count is noisy) Happy should have noticeably more
    // notes.
    size_t happyNotes = 0, spookyNotes = 0;
    const uint32_t bars = 20;
    for (uint32_t i = 0; i < bars; i++) {
        happyNotes += generateBar(Mood::Happy, 5, i, 22050).notes.size();
        spookyNotes += generateBar(Mood::Spooky, 5, i, 22050).notes.size();
    }
    CHECK(happyNotes > spookyNotes);
}

int main() {
    printf("test_music:\n");
    RUN(test_euclidean_edge_cases);
    RUN(test_euclidean_hit_count_and_spread);
    RUN(test_euclidean_deterministic);
    RUN(test_rng_deterministic_and_ranged);
    RUN(test_generate_bar_deterministic);
    RUN(test_generate_bar_varies_across_bars_and_seeds);
    RUN(test_generate_bar_notes_stay_in_bounds);
    RUN(test_mood_tempo_and_density_differ);
    printf("%d checks, %d failures\n", mt_checks, mt_failures);
    return mt_failures ? 1 : 0;
}
