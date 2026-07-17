#include "platform/render.h"

#include "platform/log.h"

namespace platform {

namespace {
constexpr float kStereoMaxParallaxPx = 6.0f;
}

float stereoShift(float depth, int eye) {
    float slider = osGet3DSliderState();
    float px = slider * kStereoMaxParallaxPx * depth;
    return eye ? px : -px;
}

bool Renderer::init() {
    Result romfsRes = romfsInit();
    LOG("romfsInit: 0x%08lX", (unsigned long)romfsRes);
    gfxInitDefault();
    gfxSet3D(true);
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    LOG("gfx/C3D/C2D init done");

    top_ = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    topRight_ = C2D_CreateScreenTarget(GFX_TOP, GFX_RIGHT);
    bottom_ = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    LOG("targets: top=%p right=%p bottom=%p", (void*)top_, (void*)topRight_, (void*)bottom_);

    atlas_ = C2D_SpriteSheetLoad("romfs:/gfx/atlas.t3x");
    textBuf_ = C2D_TextBufNew(4096);
    LOG("atlas=%p textBuf=%p", (void*)atlas_, (void*)textBuf_);

    // Query the console's actual region instead of hardcoding one - loading
    // a region's font blob that doesn't match/exist on this console can
    // silently fail (C2D_FontLoadSystem returns NULL), and every
    // C2D_TextFontParse/DrawText call after that "succeeds" while
    // rendering zero glyphs - a blank screen with no error anywhere (same
    // gotcha homeassist-ds's main.c works around).
    cfguInit();
    u8 region = CFG_REGION_USA;
    if (R_FAILED(CFGU_SecureInfoGetRegion(&region))) region = CFG_REGION_USA;
    font_ = C2D_FontLoadSystem(static_cast<CFG_Region>(region));
    // NULL is the NORMAL result on JPN/USA/EUR/AUS: those regions use the
    // shared system font, which C2D text APIs select when font==NULL (the
    // headers document "NULL for system font" throughout). Only CHN/KOR/TWN
    // get a real handle. Make sure the shared font is mapped either way.
    if (!font_) fontEnsureMapped();
    LOG("font=%p (region=%d; NULL = shared system font, normal for USA/EUR/JPN)", (void*)font_, region);

    return atlas_ != nullptr;
}

void Renderer::shutdown() {
    if (font_) {
        C2D_FontFree(font_);
        font_ = nullptr;
    }
    if (textBuf_) {
        C2D_TextBufDelete(textBuf_);
        textBuf_ = nullptr;
    }
    if (atlas_) {
        C2D_SpriteSheetFree(atlas_);
        atlas_ = nullptr;
    }
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    cfguExit();
    romfsExit();
}

void Renderer::beginFrame() const {
    if (textBuf_) C2D_TextBufClear(textBuf_);
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
}

void Renderer::endFrame() const {
    C3D_FrameEnd(0);
}

void Renderer::beginTop(int eye, uint32_t clearColor) const {
    C3D_RenderTarget* target = eye ? topRight_ : top_;
    C2D_TargetClear(target, clearColor);
    C2D_SceneBegin(target);
}

void Renderer::beginBottom(uint32_t clearColor) const {
    C2D_TargetClear(bottom_, clearColor);
    C2D_SceneBegin(bottom_);
}

void Renderer::drawSprite(int index, float x, float y, float stereoDepth, int eye, float scale) const {
    C2D_Image img = C2D_SpriteSheetGetImage(atlas_, static_cast<size_t>(index));
    C2D_DrawImageAt(img, x + stereoShift(stereoDepth, eye), y, 0.0f, nullptr, scale, scale);
}

void Renderer::drawSpriteFlipped(int index, float x, float y, float stereoDepth, int eye,
                                 float scale) const {
    C2D_Image img = C2D_SpriteSheetGetImage(atlas_, static_cast<size_t>(index));
    // Negative x-scale mirrors around the draw point, so anchor at the
    // sprite's right edge to keep x meaning "left edge on screen".
    float w = static_cast<float>(img.subtex->width) * scale;
    C2D_DrawImageAt(img, x + w + stereoShift(stereoDepth, eye), y, 0.0f, nullptr, -scale, scale);
}

void Renderer::drawSpriteFlat(int index, float x, float y, float scale) const {
    C2D_Image img = C2D_SpriteSheetGetImage(atlas_, static_cast<size_t>(index));
    C2D_DrawImageAt(img, x, y, 0.0f, nullptr, scale, scale);
}

void Renderer::drawSpriteTinted(int index, float x, float y, float stereoDepth, int eye, float scale,
                                uint32_t tintColor, float blend) const {
    C2D_Image img = C2D_SpriteSheetGetImage(atlas_, static_cast<size_t>(index));
    C2D_ImageTint tint;
    C2D_PlainImageTint(&tint, tintColor, blend);
    C2D_DrawImageAt(img, x + stereoShift(stereoDepth, eye), y, 0.0f, &tint, scale, scale);
}

// font_ may be NULL - C2D_TextFontParse treats NULL as "use the shared
// system font", which is exactly what USA/EUR/JPN consoles are supposed to
// use. Only a missing text buffer is an actual can't-draw condition.
void Renderer::drawText(const char* text, float x, float y, float stereoDepth, int eye, float scale, uint32_t color) const {
    if (!textBuf_) return;
    C2D_Text t;
    C2D_TextFontParse(&t, font_, textBuf_, text);
    C2D_TextOptimize(&t);
    C2D_DrawText(&t, C2D_WithColor, x + stereoShift(stereoDepth, eye), y, 0.0f, scale, scale, color);
}

void Renderer::drawTextFlat(const char* text, float x, float y, float scale, uint32_t color) const {
    if (!textBuf_) return;
    C2D_Text t;
    C2D_TextFontParse(&t, font_, textBuf_, text);
    C2D_TextOptimize(&t);
    C2D_DrawText(&t, C2D_WithColor, x, y, 0.0f, scale, scale, color);
}

} // namespace platform
