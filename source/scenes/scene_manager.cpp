#include "scenes/scene_manager.h"

namespace scenes {

void SceneManager::switchTo(IScene* scene) {
    if (current_) current_->onExit();
    current_ = scene;
    if (current_) current_->onEnter();
}

void SceneManager::update(float dt, const platform::InputState& input) {
    if (current_) current_->update(dt, input);
}

void SceneManager::draw(const platform::Renderer& renderer) const {
    if (current_) current_->draw(renderer);
}

} // namespace scenes
