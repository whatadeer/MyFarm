#include "core/chip_synth.h"

#include <algorithm>
#include <cmath>

namespace core {

namespace {

// Square wave, 50% duty - the classic NES pulse-channel tone. `phase` is in
// cycles (not radians): offset-into-note * freq / sampleRate.
float square(float phase) {
    float f = phase - std::floor(phase);
    return f < 0.5f ? 1.0f : -1.0f;
}

float triangle(float phase) {
    float f = phase - std::floor(phase);
    return 4.0f * std::fabs(f - 0.5f) - 1.0f;
}

// Hashed noise, coarsened to a lower "update rate" than the sample rate -
// that coarsening is what mimics the NES noise channel's LFSR (which steps
// far slower than 22050Hz) and gives chiptune noise its gritty character
// instead of sounding like smooth white noise. Indexable at any absolute
// sample position, unlike a sequential LFSR, which is what keeps this
// stateless across chunk boundaries.
float noise(uint32_t sampleIndex) {
    uint32_t coarse = sampleIndex >> 3;
    uint32_t h = coarse * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return (static_cast<float>(h & 0xFFFFu) / 32767.5f) - 1.0f;
}

// Linear attack/release so notes don't click at their boundaries - cheap,
// and unnoticeable at chiptune note lengths.
float envelope(uint32_t posInNote, uint32_t noteLen) {
    constexpr uint32_t kRampSamples = 96; // ~4.3ms at 22050Hz
    uint32_t ramp = std::min(kRampSamples, noteLen / 2);
    if (ramp == 0) return 1.0f;
    if (posInNote < ramp) return static_cast<float>(posInNote) / static_cast<float>(ramp);
    uint32_t fromEnd = noteLen - posInNote;
    if (fromEnd < ramp) return static_cast<float>(fromEnd) / static_cast<float>(ramp);
    return 1.0f;
}

} // namespace

void renderChunk(const Bar& bar, uint32_t chunkStartSample, uint32_t chunkLen, uint32_t sampleRate,
                  int16_t* out) {
    static float mix[kMaxChunkSamples];
    std::fill(mix, mix + chunkLen, 0.0f);
    uint32_t chunkEnd = chunkStartSample + chunkLen;

    for (const Note& note : bar.notes) {
        uint32_t noteEnd = note.startSample + note.lengthSamples;
        uint32_t overlapStart = std::max(chunkStartSample, note.startSample);
        uint32_t overlapEnd = std::min(chunkEnd, noteEnd);
        if (overlapStart >= overlapEnd) continue;

        for (uint32_t s = overlapStart; s < overlapEnd; s++) {
            uint32_t posInNote = s - note.startSample;
            float env = envelope(posInNote, note.lengthSamples);
            float value;
            if (note.wave == Wave::Noise) {
                value = noise(s);
            } else {
                float phase = static_cast<float>(posInNote) * note.freqHz / static_cast<float>(sampleRate);
                value = (note.wave == Wave::Square) ? square(phase) : triangle(phase);
            }
            mix[s - chunkStartSample] += value * note.volume * env;
        }
    }

    for (uint32_t i = 0; i < chunkLen; i++) {
        float v = mix[i] * 32767.0f;
        v = std::max(-32768.0f, std::min(32767.0f, v));
        out[i] = static_cast<int16_t>(v);
    }
}

} // namespace core
