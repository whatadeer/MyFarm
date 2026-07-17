#pragma once

#include "platform/input.h"
#include "platform/render.h"

namespace scenes {

class IScene {
public:
    virtual ~IScene() = default;

    virtual void onEnter() {}
    virtual void onExit() {}
    virtual void update(float dt, const platform::InputState& input) = 0;
    virtual void draw(const platform::Renderer& renderer) const = 0;
};

} // namespace scenes
