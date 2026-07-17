#include "scenes/title_scene.h"

#include "atlas.h"
#include "core/clock.h"
#include "platform/audio.h"
#include "platform/log.h"
#include "platform/save_io.h"
#include "scenes/scene_manager.h"
#include "scenes/world_scene.h"

namespace scenes {

TitleScene::TitleScene(core::GameState** gameStateSlot, WorldScene* worldScene, SceneManager* manager)
    : gameStateSlot_(gameStateSlot), worldScene_(worldScene), manager_(manager), loadedState_(0) {}

void TitleScene::onEnter() {
    hasSave_ = platform::loadFromDisk(&loadedState_);
    LOG("title onEnter: hasSave=%d", hasSave_ ? 1 : 0);
    selection_ = hasSave_ ? 1 : 0;
    platform::setMusicMood(core::Mood::Happy);
}

void TitleScene::update(float /*dt*/, const platform::InputState& input) {
    if (hasSave_ && (input.upPressed || input.downPressed)) {
        selection_ = 1 - selection_;
    }

    if (input.confirmPressed) {
        if (selection_ == 1 && hasSave_) {
            continueGame();
        } else {
            startNewGame();
        }
    }
}

void TitleScene::startNewGame() {
    uint32_t seed = static_cast<uint32_t>(core::nowSeconds());
    LOG("startNewGame: seed=%lu", (unsigned long)seed);
    *gameStateSlot_ = new core::GameState(core::GameState::newGame(seed));
    worldScene_->setState(*gameStateSlot_);
    manager_->switchTo(worldScene_);
    LOG("startNewGame: switched to world scene");
}

void TitleScene::continueGame() {
    LOG("continueGame");
    *gameStateSlot_ = new core::GameState(std::move(loadedState_));
    worldScene_->setState(*gameStateSlot_);
    manager_->switchTo(worldScene_);
    LOG("continueGame: switched to world scene");
}

void TitleScene::draw(const platform::Renderer& renderer) const {
    for (int eye = 0; eye < 2; eye++) {
        renderer.beginTop(eye, C2D_Color32(0x1a, 0x2a, 0x14, 0xFF));
        renderer.drawSprite(atlas_prop_tree_idx, 40.0f, 30.0f, 0.6f, eye, 3.0f);
        renderer.drawSprite(atlas_crop_wheat_3_idx, 320.0f, 90.0f, 0.4f, eye, 3.0f);
        renderer.drawSprite(atlas_player_down_0_idx, 180.0f, 60.0f, 0.5f, eye, 3.0f);
        renderer.drawText("MyFarm", 140.0f, 20.0f, 0.2f, eye, 1.0f, C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF));

        uint32_t newGameColor = selection_ == 0 ? C2D_Color32(0xFF, 0xE8, 0x80, 0xFF) : C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF);
        renderer.drawText("New Game", 160.0f, 160.0f, 0.2f, eye, 0.7f, newGameColor);

        if (hasSave_) {
            uint32_t continueColor = selection_ == 1 ? C2D_Color32(0xFF, 0xE8, 0x80, 0xFF) : C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF);
            renderer.drawText("Continue", 160.0f, 185.0f, 0.2f, eye, 0.7f, continueColor);
        }
    }

    renderer.beginBottom(C2D_Color32(0x14, 0x1e, 0x10, 0xFF));
    renderer.drawTextFlat("A: confirm    Up/Down: select", 20.0f, 110.0f, 0.5f, C2D_Color32(0xCC, 0xCC, 0xCC, 0xFF));
}

} // namespace scenes
