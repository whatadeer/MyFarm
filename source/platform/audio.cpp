#include "platform/audio.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include <3ds.h>

#include "core/chip_synth.h"
#include "platform/log.h"

namespace platform {

namespace {

// Index order MUST match the Sfx enum.
const char* kSfxFiles[static_cast<int>(Sfx::Count)] = {
    "romfs:/sfx/till.pcm",    "romfs:/sfx/dig.pcm",      "romfs:/sfx/chop.pcm",
    "romfs:/sfx/mine.pcm",    "romfs:/sfx/demolish.pcm", "romfs:/sfx/plant.pcm",
    "romfs:/sfx/place.pcm",   "romfs:/sfx/harvest.pcm",  "romfs:/sfx/levelup.pcm",
    "romfs:/sfx/deny.pcm",    "romfs:/sfx/ui.pcm",       "romfs:/sfx/bite.pcm",
    "romfs:/sfx/splash.pcm",  "romfs:/sfx/eat.pcm",      "romfs:/sfx/hurt.pcm",
    "romfs:/sfx/hit.pcm",     "romfs:/sfx/kill.pcm",     "romfs:/sfx/bonk.pcm",
    "romfs:/sfx/mail.pcm",    "romfs:/sfx/chime.pcm",    "romfs:/sfx/tame.pcm",
    "romfs:/sfx/descend.pcm",
};

constexpr int kChannelCount = 4; // rotating pool; overlapping effects are fine
constexpr float kSampleRate = 22050.0f;

bool gReady = false;
void* gBuffers[static_cast<int>(Sfx::Count)] = {};
unsigned gSizes[static_cast<int>(Sfx::Count)] = {};
int gNextChannel = 0;
// One waveBuf per channel - a new playSfx() on a channel replaces whatever
// finished (or is still fading) there.
ndspWaveBuf gWaveBufs[kChannelCount];

// --- Procedural music streaming -------------------------------------------
// A dedicated ndsp channel outside the SFX pool above, fed a few chunks
// ahead by core::generateBar()+core::renderChunk() rather than a looped
// pre-rendered track. kMusicBufferCount rotating buffers give ndsp enough
// queued lookahead that a slow frame (or the generation work itself)
// doesn't starve it into an audible gap.
constexpr int kMusicChannel = 4; // one past the SFX pool's channels 0..kChannelCount-1
constexpr uint32_t kMusicSampleRate = 22050;
constexpr uint32_t kMusicChunkSamples = 2048; // ~93ms/chunk
constexpr int kMusicBufferCount = 3;          // ~279ms of queued lookahead
static_assert(kMusicChunkSamples <= core::kMaxChunkSamples,
              "music chunk size must fit chip_synth's scratch buffer");

bool gMusicReady = false;   // streaming buffers allocated ok
bool gMusicPlaying = false;
core::Mood gMusicMood = core::Mood::Calm;
core::Mood gMusicPendingMood = core::Mood::Calm;
uint32_t gMusicSeed = 0;
uint32_t gMusicBarIndex = 0;
core::Bar gMusicBar;
uint32_t gMusicBarCursor = 0;

int16_t* gMusicBuffers[kMusicBufferCount] = {};
ndspWaveBuf gMusicWaveBufs[kMusicBufferCount];

// Renders exactly `count` samples into dest, advancing (and regenerating,
// as needed) the bar cursor - may cross one or more bar boundaries within a
// single chunk for very short/fast bars. A pending mood change (see
// setMusicMood()) is only ever adopted right at a bar boundary, so a mood
// switch never cuts a phrase off mid-note.
void fillMusicChunk(int16_t* dest, uint32_t count) {
    uint32_t written = 0;
    while (written < count) {
        if (gMusicBarCursor >= gMusicBar.totalSamples) {
            gMusicMood = gMusicPendingMood;
            gMusicBar = core::generateBar(gMusicMood, gMusicSeed, gMusicBarIndex++, kMusicSampleRate);
            gMusicBarCursor = 0;
        }
        uint32_t remain = gMusicBar.totalSamples - gMusicBarCursor;
        uint32_t chunk = std::min(count - written, remain);
        core::renderChunk(gMusicBar, gMusicBarCursor, chunk, kMusicSampleRate, dest + written);
        gMusicBarCursor += chunk;
        written += chunk;
    }
    DSP_FlushDataCache(dest, count * sizeof(int16_t));
}

// Fills `buf` with a fresh chunk and (re)submits it to the music channel -
// shared by the startup fill (setMusicMood's first call) and the
// steady-state refill (updateMusic()).
void submitMusicChunk(int index) {
    fillMusicChunk(gMusicBuffers[index], kMusicChunkSamples);
    ndspWaveBuf& wb = gMusicWaveBufs[index];
    wb.data_vaddr = gMusicBuffers[index];
    wb.nsamples = kMusicChunkSamples;
    wb.looping = false;
    ndspChnWaveBufAdd(kMusicChannel, &wb);
}

} // namespace

bool audioInit() {
    Result rc = ndspInit();
    if (R_FAILED(rc)) {
        LOG("ndspInit failed 0x%08lX (no dspfirm.cdc? sound disabled)", (unsigned long)rc);
        return false;
    }

    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    std::memset(gWaveBufs, 0, sizeof(gWaveBufs));

    // Load every clip into linear (DSP-visible) memory up front - they're
    // all short trims, well under ~200KB combined.
    for (int i = 0; i < static_cast<int>(Sfx::Count); i++) {
        FILE* f = fopen(kSfxFiles[i], "rb");
        if (!f) {
            LOG("sfx missing: %s", kSfxFiles[i]);
            continue;
        }
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (size > 0) {
            void* buf = linearAlloc(static_cast<size_t>(size));
            if (buf && fread(buf, 1, static_cast<size_t>(size), f) == static_cast<size_t>(size)) {
                DSP_FlushDataCache(buf, static_cast<u32>(size));
                gBuffers[i] = buf;
                gSizes[i] = static_cast<unsigned>(size);
            } else if (buf) {
                linearFree(buf);
            }
        }
        fclose(f);
    }

    // Music streaming buffers - separate from the SFX pool above and
    // allowed to fail independently (a low-memory Homebrew Launcher run
    // just plays without background music rather than losing SFX too).
    std::memset(gMusicWaveBufs, 0, sizeof(gMusicWaveBufs));
    gMusicReady = true;
    for (int16_t*& buf : gMusicBuffers) {
        void* p = linearAlloc(kMusicChunkSamples * sizeof(int16_t));
        if (!p) {
            LOG("music buffer alloc failed - background music disabled");
            gMusicReady = false;
            break;
        }
        buf = static_cast<int16_t*>(p);
    }
    if (!gMusicReady) {
        for (int16_t*& buf : gMusicBuffers) {
            if (buf) {
                linearFree(buf);
                buf = nullptr;
            }
        }
    }

    gReady = true;
    LOG("audio ready");
    return true;
}

void audioShutdown() {
    if (!gReady) return;
    for (int ch = 0; ch < kChannelCount; ch++) {
        ndspChnReset(ch);
    }
    for (void*& buf : gBuffers) {
        if (buf) {
            linearFree(buf);
            buf = nullptr;
        }
    }

    if (gMusicPlaying) {
        ndspChnReset(kMusicChannel);
        gMusicPlaying = false;
    }
    for (int16_t*& buf : gMusicBuffers) {
        if (buf) {
            linearFree(buf);
            buf = nullptr;
        }
    }
    gMusicReady = false;

    ndspExit();
    gReady = false;
}

void playSfx(Sfx sfx) {
    if (!gReady) return;
    int i = static_cast<int>(sfx);
    if (!gBuffers[i]) return;

    int ch = gNextChannel;
    gNextChannel = (gNextChannel + 1) % kChannelCount;

    ndspChnReset(ch);
    ndspChnSetInterp(ch, NDSP_INTERP_LINEAR);
    ndspChnSetRate(ch, kSampleRate);
    ndspChnSetFormat(ch, NDSP_FORMAT_MONO_PCM16);

    ndspWaveBuf& wb = gWaveBufs[ch];
    std::memset(&wb, 0, sizeof(wb));
    wb.data_vaddr = gBuffers[i];
    wb.nsamples = gSizes[i] / 2; // PCM16 mono: 2 bytes per sample
    ndspChnWaveBufAdd(ch, &wb);
}

void setMusicMood(core::Mood mood) {
    if (!gReady || !gMusicReady) return;

    gMusicPendingMood = mood;
    if (gMusicPlaying) return; // adopted at the next bar boundary, see fillMusicChunk()

    // First call: open the stream now. Seeded from the console's free-
    // running tick counter so two play sessions don't open on the same
    // phrase.
    gMusicMood = mood;
    gMusicSeed = static_cast<uint32_t>(svcGetSystemTick()) ^ 0xA5A5A5A5u;
    gMusicBarIndex = 0;
    gMusicBar = core::generateBar(gMusicMood, gMusicSeed, gMusicBarIndex++, kMusicSampleRate);
    gMusicBarCursor = 0;

    ndspChnReset(kMusicChannel);
    ndspChnSetInterp(kMusicChannel, NDSP_INTERP_LINEAR);
    ndspChnSetRate(kMusicChannel, static_cast<float>(kMusicSampleRate));
    ndspChnSetFormat(kMusicChannel, NDSP_FORMAT_MONO_PCM16);

    gMusicPlaying = true;
    for (int i = 0; i < kMusicBufferCount; i++) {
        submitMusicChunk(i);
    }
}

void stopMusic() {
    if (!gMusicPlaying) return;
    ndspChnReset(kMusicChannel);
    gMusicPlaying = false;
}

void updateMusic() {
    if (!gMusicPlaying) return;
    for (int i = 0; i < kMusicBufferCount; i++) {
        if (gMusicWaveBufs[i].status == NDSP_WBUF_FREE || gMusicWaveBufs[i].status == NDSP_WBUF_DONE) {
            submitMusicChunk(i);
        }
    }
}

} // namespace platform
