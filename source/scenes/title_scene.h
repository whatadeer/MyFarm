#pragma once

#include "core/game_state.h"
#include "scenes/scene.h"

namespace scenes {

class SceneManager;
class WorldScene;

// New Game / Continue. Constructed with pointers rather than owning its
// targets - main.cpp owns the actual GameState (created here, handed off
// on confirm) and the WorldScene/SceneManager it hands off to.
class TitleScene : public IScene {
public:
    TitleScene(core::GameState** gameStateSlot, WorldScene* worldScene, SceneManager* manager);

    void onEnter() override;
    void update(float dt, const platform::InputState& input) override;
    void draw(const platform::Renderer& renderer) const override;

private:
    void startNewGame();
    void continueGame();

    core::GameState** gameStateSlot_;
    WorldScene* worldScene_;
    SceneManager* manager_;

    bool hasSave_ = false;
    core::GameState loadedState_;
    int selection_ = 0; // 0 = New Game, 1 = Continue
};

} // namespace scenes
