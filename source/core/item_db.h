#pragma once

#include <cstdint>

namespace core {

using ItemId = uint16_t;

// The complete item set. Tools are unlimited-use and never consumed except
// the Watering Can, which swaps between its empty/full forms. Tilling
// needs the Hoe (kItemHoe, appended below - the Basic pack had a real hoe
// icon all along); hole-digging (B) stays a toolless contextual action,
// as do harvesting and foraging.
enum ItemIdValue : ItemId {
    kItemNone = 0,
    // Tools (the new-game kit - the one thing handed to the player).
    kItemAxe,
    kItemPickaxe,
    kItemHammer,
    kItemWateringCan,
    kItemWateringCanFull,
    kItemFishingRod,
    // Seeds & crops.
    kItemWheatSeed,
    kItemWheat,
    kItemTurnipSeed,
    kItemTurnip,
    kItemCarrotSeed,
    kItemCarrot,
    kItemTomatoSeed,
    kItemTomato,
    kItemPumpkinSeed,
    kItemPumpkin,
    // Gathered materials & foods.
    kItemWood,
    kItemStone,
    kItemOre,
    kItemSapling,
    kItemBerries,
    kItemMushroom,
    kItemHay,
    kItemApple,
    // Animal & producer products.
    kItemEgg,
    kItemMilk,
    kItemHoney,
    // Fishing catches (small/medium/big).
    kItemFishSmall,
    kItemFishMed,
    kItemFishBig,
    // Placeables (crafted).
    kItemFence,
    kItemGate,
    kItemPath,
    kItemBridge,
    kItemCamp,
    kItemCoop,
    kItemBarn,
    kItemChest,
    kItemWell,
    kItemBeehive,
    kItemCampfire,
    kItemLamp,
    kItemChair,
    kItemRug,
    kItemSign,
    kItemMailbox,
    // Modular house building + festive decor (appended so existing v4
    // saves' ItemIds stay valid).
    kItemWall,
    kItemFloor,
    kItemDoor,
    kItemXmasTree,
    // The Mine (Milestone 3): deep-earth treasure, healing potions, and
    // the shaft you dig to get down there. Appended - still v4-safe.
    kItemCoal,
    kItemRuby,
    kItemDiamond,
    kItemEmerald,
    kItemAmethyst,
    kItemPotion,
    kItemMineShaft,
    // The Living World update: fruit variety (a tier-2 tree drops the
    // fruit its art shows) and the rest of the premium crop catalogue.
    kItemOrange,
    kItemPear,
    kItemPeach,
    kItemCauliflowerSeed,
    kItemCauliflower,
    kItemEggplantSeed,
    kItemEggplant,
    kItemLettuceSeed,
    kItemLettuce,
    kItemRadishSeed,
    kItemRadish,
    kItemBeetrootSeed,
    kItemBeetroot,
    kItemStarfruitSeed,
    kItemStarfruit,
    kItemCucumberSeed,
    kItemCucumber,
    // The Grand Finale: corn, the rest of the sea life (fishing catch
    // pools), and the village-pack homes + snowman.
    kItemCornSeed,
    kItemCorn,
    kItemShrimp,
    kItemClownfish,
    kItemSnail,
    kItemCrab,
    kItemSeahorse,
    kItemOctopus,
    kItemLobster,
    kItemRay,
    kItemTurtle,
    kItemCottage,
    kItemHut,
    kItemManor,
    kItemSnowman,
    // Splitting the Gate into independently placeable leaves - kItemGate
    // becomes the left leaf, this is the right (appended, still v6-safe).
    kItemGateRight,
    // Shingled roof sections (premium roof tileset) - cap off wall runs.
    kItemRoof,
    // The rest of the path family (kItemPath is the stone one): plain
    // dirt, laid planks, and cart rails - a special type of path.
    kItemPathDirt,
    kItemPathPlank,
    kItemRail,
    // The furniture drop (Sorry pack): long rugs, beds, wooden tables.
    kItemRugLong,
    kItemBed,
    kItemWoodTable,
    kItemDresser,
    kItemStool,
    kItemBench,
    // The Hoe - tilling is a real tool action now (appended, save-safe).
    kItemHoe,
    // The Workbench: crafting station. New games start with only an Axe;
    // every other tool is crafted here (the bench itself is built
    // straight from a pile of wood - no hammer needed).
    kItemWorkbench,
    // The Homestead update: animal-area furnishings (feed trough, hay
    // bales, water tray), waterside decor (rowboat, picnic spread),
    // festive presents, and silver/gold chest tiers (fancier skins on
    // the same chest storage). Appended - save-safe.
    kItemTrough,
    kItemHayBale,
    kItemWaterTray,
    kItemBoat,
    kItemPicnic,
    kItemPresent,
    kItemChestSilver,
    kItemChestGold,
    // The Clone Mirror: a crystal-powered double of yourself (one only!)
    // that works the land on command - see WorldScene's clone logic.
    kItemClone,
    // Sunflower - the 14th crop (Sorry pack plants v2; tall like corn).
    kItemSunflowerSeed,
    kItemSunflower,
    kItemCount,
};

enum class ItemCategory : uint8_t {
    Tool,
    Seed,
    Crop,
    Material,
    Placeable,
};

struct ItemDef {
    const char* name;
    ItemCategory category;
    uint16_t maxStack;
};

extern const ItemDef kItemTable[kItemCount];

} // namespace core
