#pragma once

#include <vector>

#include "core/balance.h"
#include "core/clock.h"
#include "core/game_state.h"
#include "scenes/scene.h"

namespace scenes {

// The one gameplay scene: an open, chunked world the player walks in any
// direction - farming, gathering (axe/pickaxe/hands), terrain editing
// (toolless dig/fill via B, watering can for ponds), building (hammer +
// ghost placement), and taming all happen here on the same map.
class WorldScene : public IScene {
public:
    void setState(core::GameState* state) {
        state_ = state;
        // Re-apply the saved bed-sleep fast-forward so "now" picks up
        // where the world left off (new games carry offset 0).
        core::setClockOffset(state ? state->clockOffset : 0);
    }

    void onEnter() override;
    void update(float dt, const platform::InputState& input) override;
    void draw(const platform::Renderer& renderer) const override;

private:
    // Wild animals are transient ambience (respawn freely, wander, despawn
    // when left behind) - deliberately not part of the save file; only
    // tamed ones persist, in GameState::animals. Frogs are pure ambience
    // (they spawn near water and can't be tamed - they're free spirits).
    struct WildAnimal {
        uint8_t kind;    // 0 = chicken, 1 = cow, 2 = frog
        uint8_t variant; // color art index (0 = classic, 1-4 = premium recolors)
        float x, y;      // feet center, world coords
        float tx, ty;    // wander target
        float moveTimer;
        bool faceLeft = false; // mirror the art when heading left
        int reqT = 0; // frames left on the "I want..." speech bubble
    };

    // Bottom-screen HUD tabs - inventory and skills each get their own full
    // screen. Building isn't a tab at all: it's driven entirely by having
    // the Hammer equipped and cycling its ghost preview with X/Y.
    enum class HudTab : uint8_t { Inventory, Skills };

    void handleFieldInput(float dt, const platform::InputState& input);
    void handlePauseInput(const platform::InputState& input);
    void handleChestInput(const platform::InputState& input);
    void handleInventoryTap(float x, float y);
    // Workbench crafting menu (tools are crafted, not handed out).
    void handleCraftInput(const platform::InputState& input);
    void drawCraftUi(const platform::Renderer& renderer) const;
    void doContextualAction();
    void doDigAction(); // B button: dig/fill a hole, un-till, or demolish a placed object
    // Floods connected dug Holes with Water starting at (x,y) - called
    // when a hole is dug beside water or water is poured next to holes.
    int floodWaterFrom(int32_t x, int32_t y);
    void teleportHome();

    void updateWildAnimals(float dt);
    void updateFishing(float dt, const platform::InputState& input);
    void resolveCatch();
    bool tryTame(WildAnimal& wild, int64_t now);
    void collectProduce(int32_t tx, int32_t ty, core::Placed building, int64_t now);
    void collectHoney(int32_t tx, int32_t ty, int64_t now);
    void gatherAt(core::Tile& tile, int32_t tx, int32_t ty, core::Decoration kind,
                  const core::NodeBalance& bal, int64_t now);
    bool placeSelected(core::Tile& tile, int32_t tx, int32_t ty, core::ItemId item);

    // Terrain-only legality for placing `item` at (tx,ty): tile rules +
    // "don't entomb yourself", no inventory/ownership check. Shared by the
    // Hammer's direct-from-materials build path and the ghost preview.
    bool canPlaceTerrain(core::ItemId item, int32_t tx, int32_t ty) const;
    // Elevation rule: stepping between tiles of different elevAt heights
    // is blocked except down/up a stair ramp (a raised rim tile whose
    // south neighbor is the low tile, with rampAt true). Shared by the
    // player and wandering animals.
    bool cliffBlocked(int32_t fromX, int32_t fromY, int32_t toX, int32_t toY) const;

    // --- Building interiors ---------------------------------------------------
    // Rooms are ordinary stamped tiles (Floor + Wall ring + a Door) in a
    // reserved band kInteriorBandY tiles south of the overworld, one room
    // per building, positions derived from the building's tile. A on a
    // building warps in; stepping on the Door warps back out.
    static constexpr int32_t kInteriorBandY = 1000000;
    // Everything south of this is interior space (rooms sit at
    // kInteriorBandY + by*48, and |by| stays far below this margin).
    static constexpr int32_t kInteriorViewY = 500000;
    bool inInterior() const;
    // The room's anchor column / origin row for a building at (bx, by).
    static void interiorAnchor(int32_t bx, int32_t by, int32_t* ax, int32_t* ay);
    // Initial room size for a building kind (walls included); h==0 means
    // the building has no interior.
    static void interiorSizeFor(core::Placed kind, uint8_t* wl, uint8_t* wr, uint8_t* h);
    // (Re)stamps a room's tiles: Floor everywhere, Wall ring, Door at the
    // anchor column of the south wall. Preserves furniture on interior
    // tiles (only clears tiles that become walls or are newly annexed).
    void stampInterior(const core::InteriorData& room);
    void enterBuilding(int32_t bx, int32_t by, core::Placed kind);
    // The registry entry whose room rect contains an interior-band tile.
    core::InteriorData* roomContaining(int32_t tx, int32_t ty) const;

    // --- The Clone Mirror -----------------------------------------------------
    // A crystal double of the player (at most one). Persistent state
    // (position/task) lives in GameState::clone; the transient work
    // state below re-derives after load.
    void updateClone(float dt);
    void cloneAct(int32_t tx, int32_t ty);
    // Deposit into a chest within kCloneChestRadius, else drop on the
    // ground at/near the clone.
    void cloneDeposit(core::ItemId item, int count);
    // Pull one seed of any species from a nearby chest; returns the
    // species id or -1.
    int cloneTakeSeed();

    // Wild-patch plant stage 1..3 (3 = ripe, the only forageable stage).
    // Untouched plants cycle on a hash-staggered clock; picked ones grow
    // back through the same stages on the respawn timer.
    int wildStageAt(const core::Tile& tile, int32_t tx, int32_t ty, int64_t now) const;
    // Legacy path: place using one already-owned crafted item (backward
    // compatible with older saves that have placeable items sitting in
    // inventory/chests; nothing crafts these anymore).
    bool canPlaceGhost(core::ItemId item, int32_t tx, int32_t ty) const;
    bool canAffordCost(const core::BuildCost& cost) const;
    // Shared tile-mutation switch for both placement paths below. Returns
    // false (no-op) if `item` isn't a recognized placeable.
    bool applyPlacement(core::Tile& tile, int32_t tx, int32_t ty, core::ItemId item);
    // The Hammer's build workflow: consumes raw Wood/Stone directly, no
    // intermediate crafted item.
    bool buildFromMaterials(core::Tile& tile, int32_t tx, int32_t ty, core::ItemId item,
                             const core::BuildCost& cost);
    void cycleHudTab(int dir);
    void cycleBuildGhost(int dir);
    bool buildModeActive() const; // Hammer is the selected/equipped item

    void awardXp(core::Skill skill, int amount);

    // Sprinting: full-rim Circle Pad = run (land, Athletics) or hard swim
    // (open water, Swimming), fueled by state_->stamina.
    float maxStamina() const;
    bool sprintReady(const platform::InputState& input) const;
    // Drains (and trickles XP) when `active`, regenerates otherwise; call
    // exactly once per simulated frame from whichever movement handler ran.
    void sprintTick(bool active, bool inWater, float dt);
    void drawStaminaBar(const platform::Renderer& renderer, int eye, float y) const;
    void setStatus(const char* fmt, ...);
    bool rollPct(int pct);
    uint32_t nextRand();
    void selectItem(core::ItemId item); // move selection to wherever `item` sits
    // The currently equipped stack, or an empty one if nothing's selected.
    // Exactly one of selectedSlot_ (general grid) / selectedTool_ (tool
    // bar) is >= 0 at a time - "unequipped/empty hands" is both == -1.
    const core::ItemStack& selectedStack() const;
    void dropSelected();  // put 1 of the selected item on the ground in front of you
    void trashSelected(); // destroy 1 of the selected item, permanently

    core::Vec2f facingOffset() const;
    int playerSprite() const;
    static int spriteForCropStage(uint8_t speciesId, int stage, bool watered = false);
    static int spriteForItem(core::ItemId item);
    static float iconScaleForItem(core::ItemId item);
    static void footprintOffsetForItem(core::ItemId item, float* offX, float* offY);

    // What A and B would do right now, as short labels for the top-screen
    // prompt bar (empty string = no action / hide the prompt).
    void contextPrompts(char* aOut, size_t aN, char* bOut, size_t bN) const;
    void drawPromptBar(const platform::Renderer& renderer, int eye, const char* aLbl,
                       const char* bLbl) const;

    void drawWorld(const platform::Renderer& renderer, int eye) const;
    void drawBuildGhost(const platform::Renderer& renderer, int eye) const;
    // With a seed (or sapling) selected: a tinted sprout preview on the
    // faced tile, green where it can be planted, red where it can't.
    void drawSeedGhost(const platform::Renderer& renderer, int eye) const;
    void drawHud(const platform::Renderer& renderer) const;
    void drawTabHeader(const platform::Renderer& renderer) const;
    void drawInventoryTab(const platform::Renderer& renderer) const;
    void drawSkillsTab(const platform::Renderer& renderer) const;
    void drawChestUi(const platform::Renderer& renderer) const;
    void drawPauseMenu(const platform::Renderer& renderer) const;

    core::GameState* state_ = nullptr;
    int selectedSlot_ = 0;  // index into state_->inventory, or -1
    int selectedTool_ = -1; // index into state_->toolBelt, or -1
    bool paused_ = false;
    int pauseSelection_ = 0;

    HudTab hudTab_ = HudTab::Inventory;
    int buildGhostIdx_ = 0; // index into kBuildables[], which building the Hammer is loaded with

    // Workbench crafting menu (bottom screen modal).
    bool craftOpen_ = false;
    int craftSel_ = 0;

    // Chest-transfer UI (bottom screen) for the chest at (chestX_, chestY_).
    bool chestOpen_ = false;
    int32_t chestX_ = 0;
    int32_t chestY_ = 0;

    int animFrame_ = 0;    // free-running frame counter for water/walk cycles
    int actionTimer_ = 0;  // frames left of the tool-swing pose
    bool moving_ = false;
    bool swimming_ = false; // standing on open water (overworld): swim!
    bool exhausted_ = false; // stamina hit 0 - no sprinting until it
                             // refills to kExhaustRecoverFrac of max
    float sprintXpAcc_ = 0.0f; // fractional sprint XP awaiting a whole point
    // Emote bubble over the player's head (heart on tame, cheer on level).
    // On a level-up, emoteExtra_ carries the skill's signature tool/item
    // icon, drawn beside the cheer.
    int emoteSprite_ = -1;
    int emoteExtra_ = -1;
    int emoteT_ = 0;
    // Velocity (tiles/sec) - equals the input direction on normal ground,
    // but on Snowlands ice it carries momentum: slow to steer, keeps
    // gliding when you let go. Slippy.
    float velX_ = 0.0f;
    float velY_ = 0.0f;
    // Minecart riding (overworld rails): the cart follows the track,
    // turning at corners, until it runs out or the player hops off.
    bool riding_ = false;
    int rideDirX_ = 0, rideDirY_ = 0;
    int32_t rideDecidedX_ = 0, rideDecidedY_ = 0; // tile whose turn was already chosen

    char statusMsg_[96] = {0};
    int statusFrames_ = 0;

    // Fishing minigame: 0 = idle, 1 = waiting for a bite, 2 = bite window.
    int fishState_ = 0;
    float fishTimer_ = 0.0f;

    std::vector<WildAnimal> wild_;
    float spawnTimer_ = 0.0f;
    uint32_t rngState_ = 1;

    // Tree-chop leaf-poof effects: transient, visual only (the premium
    // tree-fall animation plays over the tile for ~0.8s after a chop).
    struct TreePoof {
        int32_t tx, ty;
        float t;
    };
    std::vector<TreePoof> poofs_;

    // Clone work state (transient - rebuilt from scans after load).
    float cloneCd_ = 0.0f;     // seconds until the next action
    float cloneScanT_ = 0.0f;  // seconds until the next target scan
    float cloneStuckT_ = 0.0f; // time spent failing to reach the target
    bool cloneHasTarget_ = false;
    int32_t cloneTX_ = 0, cloneTY_ = 0;
    int cloneDir_ = 0; // Facing index for the walk anim
    bool cloneMoving_ = false;

    // --- The Mine (Milestone 3) ---------------------------------------------
    // Underground floors are transient roguelike rooms: regenerated fresh
    // on every descent, never saved - only what you carry out persists.
    // mineFloor_ == 0 means overworld. The overworld player position stays
    // parked at the shaft while minePos_ tracks movement below.
    static constexpr int kMineW = 26;
    static constexpr int kMineH = 16;
    // Cell kinds: 0 floor, 1 wall, 2 hole-down, 3 exit-up, 4 rail,
    // 5 cart, 10+n = ore node of type n (0..7).
    struct MineEnemy {
        uint8_t kind; // 0 = slime, 1 = bat
        float x, y;
        int hp;
        float hurtT;   // hit-flash timer
        float aiT;     // slime: time to next hop; bat: wobble phase
        float vx, vy;  // current velocity (incl. knockback)
        bool hopping;
    };
    int mineFloor_ = 0;
    uint8_t mine_[kMineH][kMineW] = {};
    std::vector<MineEnemy> foes_;
    core::Vec2f minePos_;
    int hp_ = core::kMaxHp;
    float invulnT_ = 0.0f;
    float atkT_ = 0.0f;
    float pKnockX_ = 0.0f, pKnockY_ = 0.0f;

    void enterMine();
    void exitMine(const char* msg);
    void generateMineFloor();
    bool mineBlocked(float x, float y) const;
    void updateMine(float dt, const platform::InputState& input);
    void doMineAction();
    void attackSwing();
    bool tryEat();
    void hurtPlayer(float fromX, float fromY);
    void drawMine(const platform::Renderer& renderer, int eye) const;
};

} // namespace scenes
