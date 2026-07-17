#include "platform/screenshot.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <vector>

#include <sys/stat.h>

#include <3ds.h>

namespace platform {

namespace {

void putU32(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>(v);
    p[1] = static_cast<uint8_t>(v >> 8);
    p[2] = static_cast<uint8_t>(v >> 16);
    p[3] = static_cast<uint8_t>(v >> 24);
}

// The 3DS top framebuffer is stored sideways: 240 wide x 400 tall,
// column-major relative to the screen, GSP_BGR8_OES (3 bytes per pixel).
// Screen pixel (x, y) with x in [0,400) and y in [0,240) lives at
// ((x * 240) + (239 - y)) * 3, bytes in B,G,R order - which is exactly
// what a 24-bit BMP wants, so rows copy over without any per-pixel
// swizzling beyond the address math.
bool writeBmp(const char* path, const uint8_t* fbLeft, const uint8_t* fbRight) {
    const int eyes = fbRight ? 2 : 1;
    const int outW = 400 * eyes;
    const int outH = 240;
    const uint32_t rowBytes = static_cast<uint32_t>(outW) * 3; // multiple of 4 for 400/800
    const uint32_t dataSize = rowBytes * outH;

    FILE* f = fopen(path, "wb");
    if (!f) return false;

    uint8_t hdr[54];
    std::memset(hdr, 0, sizeof(hdr));
    hdr[0] = 'B';
    hdr[1] = 'M';
    putU32(hdr + 2, 54 + dataSize);
    putU32(hdr + 10, 54);
    putU32(hdr + 14, 40);
    putU32(hdr + 18, static_cast<uint32_t>(outW));
    putU32(hdr + 22, static_cast<uint32_t>(outH));
    hdr[26] = 1;  // planes
    hdr[28] = 24; // bpp
    putU32(hdr + 34, dataSize);
    if (fwrite(hdr, 1, sizeof(hdr), f) != sizeof(hdr)) {
        fclose(f);
        return false;
    }

    std::vector<uint8_t> row(rowBytes);
    for (int y = outH - 1; y >= 0; y--) { // BMP rows are bottom-up
        size_t o = 0;
        for (int eye = 0; eye < eyes; eye++) {
            const uint8_t* fb = eye ? fbRight : fbLeft;
            for (int x = 0; x < 400; x++) {
                const uint8_t* p = fb + (static_cast<size_t>(x) * 240 + (239 - y)) * 3;
                row[o++] = p[0];
                row[o++] = p[1];
                row[o++] = p[2];
            }
        }
        if (fwrite(row.data(), 1, rowBytes, f) != rowBytes) {
            fclose(f);
            return false;
        }
    }
    fclose(f);
    return true;
}

} // namespace

bool saveScreenshot(bool* wasStereo) {
    mkdir("sdmc:/3ds", 0777);
    mkdir("sdmc:/3ds/myfarm", 0777);
    mkdir("sdmc:/3ds/myfarm/screenshots", 0777);

    u16 w = 0, h = 0;
    u8* left = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &w, &h);
    if (!left) return false;

    u8* right = nullptr;
    if (osGet3DSliderState() > 0.0f) {
        right = gfxGetFramebuffer(GFX_TOP, GFX_RIGHT, &w, &h);
        if (right == left) right = nullptr; // 3D off at the gfx level
    }
    if (wasStereo) *wasStereo = right != nullptr;

    // Real wall-clock stamp (NOT the bed-sleep-shifted game clock) so
    // filenames sort by when you actually pressed the button.
    char path[96];
    snprintf(path, sizeof(path), "sdmc:/3ds/myfarm/screenshots/myfarm_%lld%s.bmp",
             static_cast<long long>(std::time(nullptr)), right ? "_3d" : "");
    return writeBmp(path, left, right);
}

} // namespace platform
