#pragma once

#include <cstdint>

#include <3ds.h>
#include <citro2d.h>
#include <citro3d.h>

namespace platform {

// citro2d/citro3d boot, the sprite atlas, and the stereoscopic dual-eye
// draw helpers - reuses the exact pattern proven in the sibling
// homeassist-ds project's source/ui_common.c (stereo_shift()): draw the
// same scene twice per frame, once per eye render target, with each
// sprite's X position offset by depth * slider (sign flipped per eye).
// Only the top screen is stereoscopic; the bottom (touch) screen renders
// once, flat.
class Renderer {
public:
    bool init();
    void shutdown();

    void beginFrame() const;
    void endFrame() const;

    // eye: 0 = left, 1 = right.
    void beginTop(int eye, uint32_t clearColor) const;
    void beginBottom(uint32_t clearColor) const;

    // stereoDepth: 0 = at the screen plane, higher = pops toward the
    // viewer. Only meaningful for something drawn inside a beginTop()
    // block - drawSpriteFlat() is for the (mono) bottom screen.
    void drawSprite(int index, float x, float y, float stereoDepth, int eye, float scale = 2.0f) const;
    // Horizontally mirrored - so side-view creatures can face the way
    // they're walking (x is still the sprite's left edge on screen).
    void drawSpriteFlipped(int index, float x, float y, float stereoDepth, int eye,
                           float scale = 2.0f) const;
    void drawSpriteFlat(int index, float x, float y, float scale = 2.0f) const;

    // Draws the sprite blended toward `tintColor` by `blend` (0..1), with
    // tintColor's alpha channel controlling overall opacity - used for the
    // build-placement ghost (green when legal, red when not).
    void drawSpriteTinted(int index, float x, float y, float stereoDepth, int eye, float scale,
                          uint32_t tintColor, float blend) const;

    void drawText(const char* text, float x, float y, float stereoDepth, int eye, float scale, uint32_t color) const;
    void drawTextFlat(const char* text, float x, float y, float scale, uint32_t color) const;

    bool ready() const { return atlas_ != nullptr; }

private:
    C3D_RenderTarget* top_ = nullptr;
    C3D_RenderTarget* topRight_ = nullptr;
    C3D_RenderTarget* bottom_ = nullptr;
    C2D_SpriteSheet atlas_ = nullptr;
    C2D_Font font_ = nullptr;
    C2D_TextBuf textBuf_ = nullptr;
};

// STEREO_MAX_PARALLAX_PX * osGet3DSliderState() * depth, sign flipped per
// eye - same technique as homeassist-ds/source/ui_common.c's
// stereo_shift(), ported here since this project draws through the
// Renderer class rather than free functions.
float stereoShift(float depth, int eye);

} // namespace platform
