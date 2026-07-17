#include <3ds.h>

#include "core/game_state.h"
#include "platform/audio.h"
#include "platform/input.h"
#include "platform/log.h"
#include "platform/render.h"
#include "platform/save_io.h"
#include "scenes/scene_manager.h"
#include "scenes/title_scene.h"
#include "scenes/world_scene.h"

// The libctru 3dsx loader gives the main thread a modest default stack
// (~32KB). The world-render loop calls generateChunk(), which builds a
// ~6KB Chunk by value, hundreds of times per frame - comfortably fine on
// its own, but leaving little headroom. Bump it well clear of any doubt so
// a deep call path can't smash the return address into a prefetch abort.
extern "C" {
unsigned int __stacksize__ = 256 * 1024;
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    platform::logInit();
    LOG("=== myfarm starting ===");

    platform::Renderer renderer;
    if (!renderer.init()) {
        // Bundled romfs asset missing/corrupt - a packaging bug, not a
        // recoverable runtime condition.
        LOG("renderer.init FAILED (atlas load) - halting");
        platform::logClose();
        svcBreak(USERBREAK_PANIC);
    }
    LOG("renderer.init ok");

    // Sound is best-effort: consoles without a DSP firmware dump just play
    // silently (see platform/audio.h).
    platform::audioInit();

    core::GameState* gameState = nullptr;
    scenes::SceneManager sceneManager;
    scenes::WorldScene worldScene;
    scenes::TitleScene titleScene(&gameState, &worldScene, &sceneManager);

    sceneManager.switchTo(&titleScene);
    LOG("title scene entered, starting main loop");

    bool wasTouching = false;
    // The 3DS LCD (and C3D_FRAME_SYNCDRAW) is vsync-locked to a fixed rate,
    // so a fixed timestep is precise enough for movement speed scaling -
    // no need to measure real frame time for that.
    constexpr float kFixedDt = 1.0f / 60.0f;

    int frame = 0;
    while (aptMainLoop()) {
        platform::InputState input = platform::pollInput(&wasTouching);
        sceneManager.update(kFixedDt, input);
        platform::updateMusic();

        renderer.beginFrame();
        sceneManager.draw(renderer);
        renderer.endFrame();

        if (frame < 3) LOG("frame %d complete", frame);
        frame++;
    }

    // HOME-menu quit reached here without going through the pause menu's
    // explicit Save - autosave whatever progress exists rather than
    // silently losing it.
    LOG("main loop exited, saving+shutting down");
    if (gameState) {
        platform::saveToDisk(*gameState);
        delete gameState;
    }

    platform::audioShutdown();
    renderer.shutdown();
    platform::logClose();
    return 0;
}
