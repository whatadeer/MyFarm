#pragma once

#include <cstdint>

namespace platform {

enum class MoveDir : uint8_t { None, Up, Down, Left, Right };

struct InputState {
    MoveDir move = MoveDir::None; // held direction, for continuous movement (walking)

    // How hard the direction is being held, 0..1: the Circle Pad's
    // deflection fraction, or a fixed mid value for the D-Pad (which can
    // therefore never sprint). 0 when `move` is None.
    float moveMag = 0.0f;

    // Edge-triggered (this-frame-only) - for menu navigation, where `move`
    // being *held* would otherwise re-fire every single frame.
    bool upPressed = false;
    bool downPressed = false;

    bool actionPressed = false;  // A - contextual field action (harvest/plant/place/use tool)
    bool confirmPressed = false; // A, reused inside menus
    bool cancelPressed = false;  // B - dig/fill/demolish in the field, cancel inside menus
    bool menuPressed = false;    // START - pause menu toggle

    // L/R shoulders - cycle the bottom-screen HUD tab (always - a hammer
    // in hand must never lock the player out of the other tabs).
    bool lPressed = false;
    bool rPressed = false;

    // X/Y - cycle which building the Hammer's placement ghost is loaded
    // with (X = previous, Y = next).
    bool xPressed = false;
    bool yPressed = false;

    // SELECT - save a screenshot (a stereoscopic pair when the 3D
    // slider is up).
    bool selectPressed = false;

    bool touching = false;
    bool touchTapped = false; // edge-triggered tap this frame
    float touchX = 0.0f;
    float touchY = 0.0f;
};

// Polls hid/touch this frame and translates it into game-level actions.
// `wasTouching` threads touch state across frames for edge-detection:
// KEY_TOUCH isn't provided by hidKeysDown() (same workaround as
// homeassist-ds's main.c) - an untouched screen always reports (0,0), so a
// tap is detected by the position going from all-zero to nonzero.
InputState pollInput(bool* wasTouching);

} // namespace platform
