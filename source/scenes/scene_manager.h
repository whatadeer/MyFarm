#pragma once

#include "scenes/scene.h"

namespace scenes {

class SceneManager {
public:
    void switchTo(IScene* scene);
    void update(float dt, const platform::InputState& input);
    void draw(const platform::Renderer& renderer) const;

private:
    IScene* current_ = nullptr;
};

} // namespace scenes
