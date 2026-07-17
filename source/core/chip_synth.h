#pragma once

#include <cstdint>

#include "core/music.h"

namespace core {

// Chunk size cap for renderChunk(): callers must pass chunkLen <=
// kMaxChunkSamples (4096 samples is ~186ms at 22050Hz, a fine streaming
// granularity). Keeps the renderer's scratch mix buffer a fixed-size stack
// array instead of a heap allocation on every call.
constexpr uint32_t kMaxChunkSamples = 4096;

// Stateless PCM16 renderer for a Bar of Notes: every sample is computed
// directly from its absolute position within the bar (oscillator phase =
// offset-into-note * freq / sampleRate, never carried across calls), so
// rendering a bar in one shot or in many small chunks produces identical
// output. That's what lets platform::audio synthesize music a chunk at a
// time, spread across frames, instead of stalling one frame to render a
// whole multi-second bar.
//
// Renders exactly chunkLen samples (the [chunkStartSample, chunkStartSample
// + chunkLen) window of `bar`) into `out`. `bar` and `out` are the caller's;
// this only reads/writes within that window.
void renderChunk(const Bar& bar, uint32_t chunkStartSample, uint32_t chunkLen, uint32_t sampleRate,
                  int16_t* out);

} // namespace core
