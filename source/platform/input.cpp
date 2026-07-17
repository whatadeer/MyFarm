#include "platform/input.h"

#include <cmath>
#include <cstdlib>

#include <3ds.h>

namespace platform {

InputState pollInput(bool* wasTouching) {
    hidScanInput();
    u32 down = hidKeysDown();
    u32 held = hidKeysHeld();

    InputState state;

    if (held & KEY_UP) {
        state.move = MoveDir::Up;
    } else if (held & KEY_DOWN) {
        state.move = MoveDir::Down;
    } else if (held & KEY_LEFT) {
        state.move = MoveDir::Left;
    } else if (held & KEY_RIGHT) {
        state.move = MoveDir::Right;
    }

    if (state.move != MoveDir::None) {
        state.moveMag = 0.6f; // D-Pad walks - sprinting needs the Circle Pad's rim
    } else {
        circlePosition circle;
        hidCircleRead(&circle);
        constexpr int kDeadzone = 40;
        if (std::abs(circle.dx) > kDeadzone && std::abs(circle.dx) > std::abs(circle.dy)) {
            state.move = circle.dx > 0 ? MoveDir::Right : MoveDir::Left;
        } else if (std::abs(circle.dy) > kDeadzone) {
            state.move = circle.dy > 0 ? MoveDir::Up : MoveDir::Down;
        }
        if (state.move != MoveDir::None) {
            // ~156 is the pad's nominal full deflection per libctru.
            float mag = std::sqrt(static_cast<float>(circle.dx) * circle.dx +
                                  static_cast<float>(circle.dy) * circle.dy) /
                        156.0f;
            state.moveMag = mag > 1.0f ? 1.0f : mag;
        }
    }

    state.upPressed = (down & KEY_UP) != 0;
    state.downPressed = (down & KEY_DOWN) != 0;

    state.actionPressed = (down & KEY_A) != 0;
    state.confirmPressed = state.actionPressed;
    state.cancelPressed = (down & KEY_B) != 0;
    state.menuPressed = (down & KEY_START) != 0;
    state.lPressed = (down & KEY_L) != 0;
    state.rPressed = (down & KEY_R) != 0;
    state.xPressed = (down & KEY_X) != 0;
    state.yPressed = (down & KEY_Y) != 0;
    state.selectPressed = (down & KEY_SELECT) != 0;

    touchPosition touch;
    hidTouchRead(&touch);
    bool isTouching = (touch.px != 0 || touch.py != 0);
    state.touching = isTouching;
    state.touchTapped = isTouching && !*wasTouching;
    state.touchX = static_cast<float>(touch.px);
    state.touchY = static_cast<float>(touch.py);
    *wasTouching = isTouching;

    return state;
}

} // namespace platform
