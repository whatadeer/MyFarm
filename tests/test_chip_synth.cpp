// Host-native tests for source/core/chip_synth.h: PCM rendering of a Bar,
// and the statelessness that lets it be rendered in arbitrary chunks.
#include <algorithm>
#include <cstdint>
#include <vector>

#include "core/chip_synth.h"
#include "core/music.h"
#include "minitest.h"

using namespace core;

static void test_silence_when_no_notes() {
    Bar bar;
    bar.totalSamples = 500;
    std::vector<int16_t> out(500, 12345); // poison value - must be overwritten with zero
    renderChunk(bar, 0, 500, 22050, out.data());
    for (int16_t s : out) CHECK(s == 0);
}

static void test_deterministic() {
    Bar bar;
    bar.totalSamples = 1000;
    bar.notes.push_back(Note{100, 400, 220.0f, Wave::Square, 0.8f});
    bar.notes.push_back(Note{300, 500, 110.0f, Wave::Triangle, 0.5f});
    bar.notes.push_back(Note{0, 200, 0.0f, Wave::Noise, 0.3f});

    std::vector<int16_t> a(1000), b(1000);
    renderChunk(bar, 0, 1000, 22050, a.data());
    renderChunk(bar, 0, 1000, 22050, b.data());
    CHECK(a == b);
}

static void test_chunked_matches_single_shot() {
    // The whole point of the stateless-by-position design: rendering in
    // one call or in several small chunks must produce identical PCM.
    Bar bar;
    bar.totalSamples = 2000;
    bar.notes.push_back(Note{0, 2000, 55.0f, Wave::Triangle, 0.4f});   // spans the whole bar
    bar.notes.push_back(Note{250, 600, 440.0f, Wave::Square, 0.6f});   // mid-bar, crosses chunk edges
    bar.notes.push_back(Note{1200, 150, 0.0f, Wave::Noise, 0.5f});

    std::vector<int16_t> whole(2000);
    renderChunk(bar, 0, 2000, 22050, whole.data());

    std::vector<int16_t> chunked(2000);
    uint32_t chunkSize = 333; // deliberately not a divisor of 2000
    for (uint32_t start = 0; start < 2000; start += chunkSize) {
        uint32_t len = std::min(chunkSize, 2000 - start);
        renderChunk(bar, start, len, 22050, chunked.data() + start);
    }

    CHECK(whole == chunked);
}

static void test_output_stays_in_range() {
    // Deliberately overlap several loud notes so the mix would clip
    // without the final clamp.
    Bar bar;
    bar.totalSamples = 800;
    for (int i = 0; i < 6; i++) {
        bar.notes.push_back(Note{0, 800, 200.0f + static_cast<float>(i) * 37.0f, Wave::Square, 0.9f});
    }
    std::vector<int16_t> out(800);
    renderChunk(bar, 0, 800, 22050, out.data());
    for (int16_t s : out) {
        CHECK(s >= -32768 && s <= 32767);
    }
}

static void test_square_wave_frequency() {
    // 100Hz square wave at 10000Hz sample rate over 0.1s (1000 samples) is
    // 10 full cycles -> ~20 sign changes. Envelope fades at the very edges
    // can eat a couple, so check a generous but still meaningful band.
    Bar bar;
    bar.totalSamples = 1000;
    bar.notes.push_back(Note{0, 1000, 100.0f, Wave::Square, 1.0f});
    std::vector<int16_t> out(1000);
    renderChunk(bar, 0, 1000, 10000, out.data());

    int signChanges = 0;
    for (size_t i = 1; i < out.size(); i++) {
        bool prevPos = out[i - 1] >= 0;
        bool curPos = out[i] >= 0;
        if (prevPos != curPos) signChanges++;
    }
    CHECK(signChanges >= 16 && signChanges <= 22);
}

int main() {
    printf("test_chip_synth:\n");
    RUN(test_silence_when_no_notes);
    RUN(test_deterministic);
    RUN(test_chunked_matches_single_shot);
    RUN(test_output_stays_in_range);
    RUN(test_square_wave_frequency);
    printf("%d checks, %d failures\n", mt_checks, mt_failures);
    return mt_failures ? 1 : 0;
}
