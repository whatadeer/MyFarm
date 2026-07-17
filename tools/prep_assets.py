"""
Slices the Sprout Lands source sheets into individual, named PNGs under
gfx/, and writes gfx/atlas.t3s (the tex3ds manifest that bin-packs all of
them into one romfs:/gfx/atlas.t3x spritesheet at build time - see the
Makefile's GRAPHICS/ROMFS/GFXBUILD rules). Each entry also becomes an
`atlas_<name>_idx` constant in the generated atlas.h, via tex3ds's -H flag.

Sources live on disk under itch/SproutLands/ (both the free "Basic pack"
and the "premium pack" - the premium pack is where the real Axe/Pickaxe/
Watering-Can/Hammer icons and the tool-tier tile variants live; the free
pack covers everything else this game uses).

Coordinates were hand-identified by visually inspecting each source sheet
(labeled grid overlays). The Sprout Lands sheets use a clean 16x16 grid
(character/building sheets use 48x48, the cow 32x32) with NO margin, so
every rect is (col*CELL, row*CELL, w, h).

Difficulty-tier variants (autumn tree, copper/gold rocks, exotic bushes),
the barn and camp buildings, the full watering can, gold ore, and the hole
tile are all generated PROCEDURALLY from base sprites via the recolor
helpers below - the pack's license explicitly allows modifying the assets.

Re-run with: python tools/prep_assets.py
Requires: Pillow (`pip install pillow`)
"""
import colorsys
import struct
import wave
from pathlib import Path

from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
GFX_DIR = ROOT / "gfx"
SFX_DIR = ROOT / "sfx"
ITCH = ROOT / "itch" / "SproutLands"
BASIC = ITCH / "Sprout Lands - Sprites - Basic pack"
PREMIUM = ITCH / "Sprout Lands - Sprites - premium pack"
SORRY = ITCH / "Sprout Sorry pack"
# Vryell's "Controllers & Keyboard" pack - button-prompt glyphs. The Switch
# sheet's A/B/X/Y match the 3DS's Nintendo layout; we use the light (cream)
# theme, column 0 (bold outline), which reads best on the dark HUD bar.
BUTTONS = ROOT / "itch" / "Controllers and Keyboard" / "controller_switch.png"

BIOM = BASIC / "Objects/Basic Grass Biom things 1.png"
CHAR = BASIC / "Characters/Basic Charakter Spritesheet.png"
ACTIONS = BASIC / "Characters/Basic Charakter Actions.png"
# The free pack's "Tools.png" and "Basic tools and meterials.png" are
# actually tool-in-hand SWING-overlay frames (the handle deliberately
# clips at the sprite's edge, meant to be composited onto the character
# during an attack animation) - not standalone icons, and there is no
# dedicated Hoe/Shovel art anywhere in either pack. The premium pack's
# Objects/Items sheets are the real flat icons: "tools and meterials.png"
# (4x3 @16px: watering can, axe, hammer, grass / slingshot, stump-sprout,
# log, stick / plank, stone, stone-block, round stone) and "All items.png"
# (8x15 @16px - its (3,0) cell is the proper curved-head pickaxe).
PREMIUM_TOOLS = PREMIUM / "Objects/Items/tools and meterials.png"
ALL_ITEMS = PREMIUM / "Objects/Items/All items.png"
TREES = PREMIUM / "Objects/Trees, stumps and bushes.png"
MUSH = PREMIUM / "Objects/Mushrooms, Flowers, Stones.png"
STONE_PATH = PREMIUM / "Tilesets/Building parts/Stone_Path.png"
# The premium "New tiles" soil sheet - the pack's canonical walking-path
# ground, and (recolored) our tilled-bed material. Standard blob layout.
SOIL = PREMIUM / "Tilesets/ground tiles/New tiles/Darker_Soil_Ground_Tiles.png"
# Elevation: the hill-plateau tileset (standard blob layout with the
# cliff lip baked into the south edges) + the stone-step stair strip.
HILLS = PREMIUM / "Tilesets/ground tiles/New tiles/Grass_Hill_Tiles_v2.png"
SLOPES = PREMIUM / "Tilesets/ground tiles/New tiles/Grass_Hill_Tiles_Slopes v.2.png"
# Hedge walls (standard blob layout) - the hedge-maze garden walls.
BUSH_TILES = PREMIUM / "Tilesets/ground tiles/New tiles/Bush_Tiles.png"
PATHS = PREMIUM / "Tilesets/Building parts/Paths.png"
HOUSES = PREMIUM / "Tilesets/Building parts/Animal Structures/Chikcen_Houses.png"
# Animal-area furnishings: feed troughs + hay bales ("Barn structures" is
# troughs/bales, not a barn building) and the 3-state water tray.
BARNS = PREMIUM / "Tilesets/Building parts/Animal Structures/Barn structures.png"
TRAY = PREMIUM / "Tilesets/Building parts/Animal Structures/Water tray.png"
# The pack's real crafting bench (drawer + tools on top) - replaces the
# old hand-composited table+mallet workbench.
WORKSTATION = PREMIUM / "Objects/work station.png"
# Tree-chop leaf-poof animation (13 frames of 64x48, pre-aligned).
TREEFALL = PREMIUM / "Objects/Tree animations/tree_fall_animation_sprite_sheet.png"
BOATS = PREMIUM / "Objects/Boats.png"
# Water scatter decor: rocks / reeds / extra lily pads (16px cells).
WATER_OBJ = PREMIUM / "Objects/Water Objects.png"
PIKNIK_BASKET = PREMIUM / "Objects/Piknik basket.png"
PIKNIK_BLANKET = PREMIUM / "Objects/Piknik blanket.png"
# Premium crop sheets: Farming Plants.png is 5x15 (row = crop, cols 0-3 =
# growth stages); the items sheet is 2x15 (seed bag, harvested) with
# matching rows. Carrot = row 2, tomato = row 4, pumpkin = row 9.
FARM_PLANTS = PREMIUM / "Objects/Farming Plants.png"
FARM_ITEMS = PREMIUM / "Objects/Items/Farming Plants items.png"
GATES = PREMIUM / "Tilesets/Building parts/Fence gates animation sprites .png"
SIGNS = PREMIUM / "Objects/signs.png"
WELL = PREMIUM / "Objects/Water well.png"
MAILBOX = PREMIUM / "Tilesets/Building parts/Mailbox Animation Frames.png"
# Sprout Sorry pack (early-access sheets + audio).
PLANT2 = SORRY / "Early Access/Plant update 2"
BEEHIVE = PLANT2 / "Bee/beehive.png"
BEE = PLANT2 / "Bee/small_bee_spritesheet.png"
HONEY = PLANT2 / "Bee/bread_jam_honey_items.png"
FROG = PLANT2 / "frog/frog_spritesheet.png"
BIRCH = PLANT2 / "Birch wood Biom.png"
CHERRY = PLANT2 / "Cherry Blossom Biom.png"
PINE = PLANT2 / "Pine Tree Biome.png"
CAMPFIRE = SORRY / "Early Access/Sprout winter/campfire.png"
FIRE = SORRY / "Early Access/Sprout winter/fire animation.png"
ROD = SORRY / "Early Access/Ocean Pack/fishing_rod.png"
FISH = SORRY / "Early Access/Ocean Pack/Fish Sprites.png"
# Dedicated fishing poses (32 cols x 6 rows of 48x48; row 2 is the
# rod-out waiting loop) + the bobber/splash frames (16px cells, the
# bobber-with-splash stacks two cells tall).
FISH_FRONT = SORRY / "Early Access/Ocean Pack/fishing animation front.png"
FISH_BACK = SORRY / "Early Access/Ocean Pack/fishing animation back.png"
FISH_SIDE = SORRY / "Early Access/Ocean Pack/fishing animation side.png"
SPLASH = SORRY / "Early Access/Ocean Pack/fishing water splash frames and rod.png"
# Ambient swimmers (15-frame 16px strips; two frames make a wiggle).
FISH_SMALL_STRIP = SORRY / "Early Access/Ocean Pack/small fish.png"
FISH_MED_STRIP = SORRY / "Early Access/Ocean Pack/mediuml fish.png"
PIKNIK_FOODS = PLANT2 / "piknik/piknik_foods.png"
# Plants v2 (Sorry pack): 2-row bands per crop; sunflower is band 7
# (rows 14-15). Its items sheet pairs two crops per row.
PLANTS_V2 = PLANT2 / "farming plants v2.png"
ITEMS_V2 = PLANT2 / "farming plants items v2.png"
AUDIO = SORRY / "Audio"
# Weather icons (premium UI pack emoji set) - replace the bottom-screen
# clock's plain "Clear"/"Raining"/"Snowing" text with real symbols.
WEATHER_ICONS = ITCH / "Sprout Lands - UI Pack - Premium pack/emojis/emoji style ui/weather/Weather_Icons_small.png"
# Winter pack (the far-north Snowlands biome) + modular house building.
WINTER = SORRY / "Early Access/Sprout winter"
SNOW1 = WINTER / "snow tiles 1.png"
ICE = WINTER / "ice tiles.png"
WINTER_SPRITES = WINTER / "winter sprites.png"
XMAS = WINTER / "christmas tree.png"
WALLS = PREMIUM / "Tilesets/Building parts/Wooden_House_Walls_Tilset.png"
ROOF = PREMIUM / "Tilesets/Building parts/Wooden_House_Roof_Tilset.png"
P_FENCES = PREMIUM / "Tilesets/Building parts/Fences.png"
DOORS = BASIC / "Tilesets/Doors.png"
WOODEN_HOUSE = BASIC / "Tilesets/Wooden House.png"
# UI pack: real slot frames for the bottom-screen inventory/chest grids.
UI_BASIC = ITCH / "Sprout Lands - UI Pack - Basic pack/Sprite sheets/Sprite sheet for Basic Pack.png"
HEARTS = ITCH / "Sprout Lands - UI Pack - Premium pack/UI Sprites/Icons/special icons/Hearts.png"
# Dungeon pack (the Mine, Milestone 3).
DUNGEON = SORRY / "Early Access/Dungeon Pack"
D_GROUND = DUNGEON / "tiles/ground_dirt_orange.png"
D_WALLS = DUNGEON / "tiles/Dungeon_walls.png"
D_ROCKS = DUNGEON / "tiles/Rocks.png"
D_ITEMS = DUNGEON / "tiles/dungeon_items.png"
D_RAILS = DUNGEON / "tiles/Rails.png"
D_CARTS = DUNGEON / "tiles/Carts.png"
D_BAT = DUNGEON / "enemies/bat_animations.png"
D_SLIME = DUNGEON / "enemies/small_green_slime_animations.png"
# Deep-mine flavor: the darker ground recolor (same blob layout as
# D_GROUND) and the crate/sack/pot props sheet.
D_GROUND_DARK = DUNGEON / "tiles/ground_dirt_orange_dark.png"
D_PROBS = DUNGEON / "tiles/dungeon_probs.png"
# Winter: animated presents (frame 0 of each color strip) + snowflake
# particles for Snowlands snowfall.
PRESENTS = [WINTER / "present red.png", WINTER / "present red 2.png",
            WINTER / "present red 3.png", WINTER / "present green.png",
            WINTER / "present green 2.png"]
SNOWFLAKES = WINTER / "snomwflakes.png"
# UI premium icons: skill stars and animal-mood faces.
STARS = ITCH / "Sprout Lands - UI Pack - Premium pack/UI Sprites/Icons/special icons/stars.png"
HAPPY = ITCH / "Sprout Lands - UI Pack - Premium pack/UI Sprites/Icons/special icons/Small Happines-Sadness icons.png"

# ---------------------------------------------------------------------------
# Plain rectangular crops: name -> (sheet, x, y, w, h)
# ---------------------------------------------------------------------------
SPRITES = {
    # --- Ground tiles ------------------------------------------------------
    # Plain grass is the interior of the big autotile blob; the _1.._6
    # variants (tufts/sprouts/pebbles/flowers, rows 5-6 of Grass.png) get
    # hash-sprinkled by worldgen for ground variety.
    "tile_grass_0": (BASIC / "Tilesets/Grass.png", 16, 16, 16, 16),
    "tile_grass_1": (BASIC / "Tilesets/Grass.png", 0, 80, 16, 16),
    "tile_grass_2": (BASIC / "Tilesets/Grass.png", 16, 80, 16, 16),
    "tile_grass_3": (BASIC / "Tilesets/Grass.png", 48, 80, 16, 16),
    "tile_grass_4": (BASIC / "Tilesets/Grass.png", 80, 80, 16, 16),
    "tile_grass_5": (BASIC / "Tilesets/Grass.png", 0, 96, 16, 16),
    "tile_grass_6": (BASIC / "Tilesets/Grass.png", 80, 96, 16, 16),
    # Plain full dirt tile (tilled ground is the at_till_* soil blob).
    "tile_dirt":         (BASIC / "Tilesets/Tilled Dirt.png", 0, 0, 16, 16),
    # 4-frame animated water.
    "tile_water_0": (BASIC / "Tilesets/Water.png", 0, 0, 16, 16),
    "tile_water_1": (BASIC / "Tilesets/Water.png", 16, 0, 16, 16),
    "tile_water_2": (BASIC / "Tilesets/Water.png", 32, 0, 16, 16),
    "tile_water_3": (BASIC / "Tilesets/Water.png", 48, 0, 16, 16),
    # Stone path tile (premium) - the Path placeable costs Stone, so the
    # ground art should read as stone, not the free pack's wooden planks.
    "tile_path":    (STONE_PATH, 16, 48, 16, 16),
    # Laid-plank scatter (premium Paths.png) - the Plank Path's overlay.
    "path_planks":  (PATHS, 0, 16, 16, 16),
    "tile_bridge":  (BASIC / "Objects/Wood Bridge.png", 0, 16, 16, 16),

    # --- Snowlands (winter pack): snow ground variants, walkable ice, and
    # snowy trees. tile_snow_1..4 = sprigs / moss / yellow / pink flowers.
    "tile_snow_0": (SNOW1, 16, 272, 16, 16),
    "tile_snow_1": (SNOW1, 0, 208, 16, 16),
    "tile_snow_2": (SNOW1, 32, 208, 16, 16),
    "tile_snow_3": (SNOW1, 144, 208, 16, 16),
    "tile_snow_4": (SNOW1, 144, 304, 16, 16),
    "tile_ice_0": (ICE, 0, 0, 16, 16),
    "tile_ice_1": (ICE, 16, 0, 16, 16),
    "tile_ice_2": (ICE, 32, 0, 16, 16),
    "tile_ice_3": (ICE, 48, 0, 16, 16),
    "tree_snow": (WINTER_SPRITES, 0, 50, 16, 32),
    # Modular walls (premium walls tileset): wall_l/wall_r are the framed
    # left/right trim columns for the ends of horizontal runs. The plain
    # mid/lone piece (place_wall) is COMPOSITED in main() - the old single
    # crop was the right half of the sheet's inset-panel wall, which
    # looked lopsided on its own.
    "wall_l": (WALLS, 0, 0, 16, 32),
    "wall_r": (WALLS, 32, 0, 16, 32),
    # Roof sections (premium roof tileset, big 4-wide group: row 2 =
    # scalloped top shingles, row 3 = ridge band, row 4 = bottom shingles).
    # Roofs are 2D areas now - each 16x32 piece pairs the row art for its
    # position-from-the-top with a shingle lower half (rows south of it
    # overdraw the lower half, so only the southernmost row's shows):
    # roof_* (band+bottom) serves rows one-below-the-top; rooftop_*
    # (scallop+bottom) the exposed top row; rooffill_* (shingles both) the
    # deep interior rows. Columns: left trim / two mids / right trim.
    "roof_l":  (ROOF, 48, 48, 16, 32),
    "roof_m":  (ROOF, 64, 48, 16, 32),
    "roof_m2": (ROOF, 80, 48, 16, 32),
    "roof_r":  (ROOF, 96, 48, 16, 32),

    # --- The Mine (dungeon pack) ---------------------------------------------
    # Orange cave floor: plain interior + speck/sprout/pebble variants.
    "mine_floor_0": (D_GROUND, 16, 16, 16, 16),
    "mine_floor_1": (D_GROUND, 0, 80, 16, 16),
    "mine_floor_2": (D_GROUND, 112, 112, 16, 16),
    "mine_floor_3": (D_GROUND, 96, 128, 16, 16),
    # Deep floors (below kMineDarkFloor) use the darker recolor sheet so
    # descending visibly changes the world.
    "mine_floor_d0": (D_GROUND_DARK, 16, 16, 16, 16),
    "mine_floor_d1": (D_GROUND_DARK, 0, 80, 16, 16),
    "mine_floor_d2": (D_GROUND_DARK, 112, 112, 16, 16),
    "mine_floor_d3": (D_GROUND_DARK, 96, 128, 16, 16),
    # Dark stone arch = the hole leading down a floor.
    # A real hole-in-the-floor tile (the dungeon pack ships four variants;
    # the old crop from the walls sheet read as a cave entrance).
    "mine_hole": (DUNGEON / "tiles/ground_dirt_orange_hole.png", 0, 0, 16, 16),
    # Ore nodes, in depth order: stone, coal, copper, gold, ruby, diamond,
    # emerald, amethyst.
    "node_stone":    (D_ROCKS, 0, 0, 16, 16),
    "node_coal":     (D_ROCKS, 0, 32, 16, 16),
    "node_copper":   (D_ROCKS, 32, 32, 16, 16),
    "node_gold":     (D_ROCKS, 48, 32, 16, 16),
    "node_ruby":     (D_ROCKS, 0, 48, 16, 16),
    "node_diamond":  (D_ROCKS, 16, 48, 16, 16),
    "node_emerald":  (D_ROCKS, 32, 48, 16, 16),
    "node_amethyst": (D_ROCKS, 48, 48, 16, 16),
    "tool_pickaxe": (D_ITEMS, 0, 0, 16, 16),
    # Treasure & potion item icons.
    "item_coal":     (D_ITEMS, 0, 16, 16, 16),
    "item_ruby":     (D_ITEMS, 16, 16, 16, 16),
    "item_diamond":  (D_ITEMS, 32, 16, 16, 16),
    "item_emerald":  (D_ITEMS, 48, 16, 16, 16),
    "item_amethyst": (D_ITEMS, 64, 16, 16, 16),
    "item_potion":   (D_ITEMS, 16, 32, 16, 16),
    # Mine-cart rails: SEAMLESS straight mids (ties run edge to edge so
    # runs link up), end-cap stubs for where a run terminates, and the
    # four quarter-turn corners (the 2x2 ring at the sheet's top-left).
    # Named by which sides connect.
    "rail_h":    (D_RAILS, 16, 32, 16, 16),
    "rail_h_l":  (D_RAILS, 0, 32, 16, 16),   # stub west, connects east
    "rail_h_r":  (D_RAILS, 32, 32, 16, 16),  # stub east, connects west
    "rail_v":    (D_RAILS, 48, 16, 16, 16),
    "rail_v_t":  (D_RAILS, 48, 0, 16, 16),   # stub north, connects south
    "rail_v_b":  (D_RAILS, 48, 32, 16, 16),  # stub south, connects north
    "rail_c_se": (D_RAILS, 0, 0, 16, 16),
    "rail_c_sw": (D_RAILS, 16, 0, 16, 16),
    "rail_c_ne": (D_RAILS, 0, 16, 16, 16),
    "rail_c_nw": (D_RAILS, 16, 16, 16, 16),
    # Player HP hearts (outlined 16px set): full / half / empty.
    "heart_full":  (HEARTS, 0, 16, 16, 16),
    "heart_half":  (HEARTS, 16, 16, 16, 16),
    "heart_empty": (HEARTS, 32, 16, 16, 16),
    # Slime frames: two idle bobs + a jump stretch.
    "slime_0": (D_SLIME, 16, 16, 16, 16),
    "slime_1": (D_SLIME, 48, 16, 16, 16),
    "slime_2": (D_SLIME, 128, 48, 16, 16),

    # --- Resource nodes & props --------------------------------------------
    # Tree tiers: t0 = the free pack's standalone small tree (complete
    # 16x32 sprite in col 0 - NOT col 1, that's the left half of the wide
    # tree); t1 = the free pack's big 2-column tree; t2 = the premium
    # pack's fruit trees (apple/orange/pear/peach, hash-picked per tile
    # for variety - see world_scene.cpp's kFruitTrees).
    "prop_tree":      (BIOM, 0, 0, 16, 32),
    "prop_tree_big":  (BIOM, 16, 0, 32, 32),
    # The ancient giant (bottom-right of the trees sheet): 44x48px of
    # canopy in a clean 3x3-cell block - the game's tree tier 3.
    "tree_ancient": (TREES, 144, 48, 48, 48),
    # Row 6 stump & deadfall variants: two small cuts, the broad and the
    # grand rooted stumps (fruit/ancient tree leftovers), and three
    # fallen logs (plain, sprouting, mossy-with-mushrooms).
    "stump_v1": (TREES, 0, 96, 16, 16),
    "stump_v2": (TREES, 16, 96, 16, 16),
    "stump_broad": (TREES, 32, 96, 32, 16),
    "stump_grand": (TREES, 64, 96, 32, 16),
    "log_small": (TREES, 96, 96, 16, 16),
    "log_sprout": (TREES, 112, 96, 32, 16),
    "log_shroom": (TREES, 144, 96, 32, 16),
    "prop_tree_fruit_0": (TREES, 48, 0, 32, 32),
    "prop_tree_fruit_1": (TREES, 80, 0, 32, 32),
    "prop_tree_fruit_2": (TREES, 112, 0, 32, 32),
    "prop_tree_fruit_3": (TREES, 144, 0, 32, 32),
    # prop_stump (chopped/regrowing tree) is a windowed crop normalized
    # below - the premium stumps row isn't cell-aligned.
    # Rock tiers: t0 = free pack rock; t1 boulder / t2 mossy boulder are
    # windowed premium crops normalized below (they escalate in physical
    # size - bigger rock, higher Mining requirement).
    "prop_rock":      (BIOM, 128, 16, 16, 16),
    "prop_pebble":    (BIOM, 96, 64, 16, 16),
    "prop_bush":      (BIOM, 0, 48, 16, 16),
    "prop_bush_empty": (BIOM, 16, 48, 16, 16),
    # Mushroom tiers: red and flat-cap purple from the premium sheet (the
    # skull mushroom t2 is a windowed crop normalized below).
    "prop_mushroom":  (MUSH, 16, 0, 16, 16),
    "prop_mushroom2": (MUSH, 80, 0, 16, 16),
    "prop_sapling":   (BIOM, 80, 16, 16, 16),
    "prop_lily":      (BIOM, 112, 64, 16, 16),

    # --- Biome trees (Sorry pack): slim 16x32 tree, big 32x32 tree, and a
    # stump per biome. Which biome's art a Tree tile uses is decided at
    # draw time from the region hash (core::biomeAt) - pure visuals.
    "tree_birch":       (BIRCH, 64, 16, 16, 32),
    "tree_birch_big":   (BIRCH, 80, 16, 32, 32),
    "stump_birch":      (BIRCH, 16, 48, 16, 16),
    "tree_cherry":      (CHERRY, 64, 16, 16, 32),
    "tree_cherry_big":  (CHERRY, 80, 16, 32, 32),
    "stump_cherry":     (CHERRY, 48, 48, 16, 16),
    "tree_pine":        (PINE, 64, 16, 16, 32),
    "tree_pine_big":    (PINE, 80, 16, 32, 32),
    "stump_pine":       (PINE, 32, 48, 16, 16),

    # --- Crops (seed bag, 4 growth stages, harvested icon) ------------------
    "crop_wheat_seed":      (BASIC / "Objects/Basic Plants.png", 0, 0, 16, 16),
    "crop_wheat_0":         (BASIC / "Objects/Basic Plants.png", 16, 0, 16, 16),
    "crop_wheat_1":         (BASIC / "Objects/Basic Plants.png", 32, 0, 16, 16),
    "crop_wheat_2":         (BASIC / "Objects/Basic Plants.png", 48, 0, 16, 16),
    "crop_wheat_3":         (BASIC / "Objects/Basic Plants.png", 64, 0, 16, 16),
    "crop_wheat_harvested": (BASIC / "Objects/Basic Plants.png", 80, 0, 16, 16),
    "crop_turnip_seed":      (BASIC / "Objects/Basic Plants.png", 0, 16, 16, 16),
    "crop_turnip_0":         (BASIC / "Objects/Basic Plants.png", 16, 16, 16, 16),
    "crop_turnip_1":         (BASIC / "Objects/Basic Plants.png", 32, 16, 16, 16),
    "crop_turnip_2":         (BASIC / "Objects/Basic Plants.png", 48, 16, 16, 16),
    "crop_turnip_3":         (BASIC / "Objects/Basic Plants.png", 64, 16, 16, 16),
    "crop_turnip_harvested": (BASIC / "Objects/Basic Plants.png", 80, 16, 16, 16),
    "crop_carrot_0":         (FARM_PLANTS, 0, 32, 16, 16),
    "crop_carrot_1":         (FARM_PLANTS, 16, 32, 16, 16),
    "crop_carrot_2":         (FARM_PLANTS, 32, 32, 16, 16),
    "crop_carrot_3":         (FARM_PLANTS, 48, 32, 16, 16),
    "crop_carrot_seed":      (FARM_ITEMS, 0, 32, 16, 16),
    "crop_carrot_harvested": (FARM_ITEMS, 16, 32, 16, 16),
    "crop_tomato_0":         (FARM_PLANTS, 0, 64, 16, 16),
    "crop_tomato_1":         (FARM_PLANTS, 16, 64, 16, 16),
    "crop_tomato_2":         (FARM_PLANTS, 32, 64, 16, 16),
    "crop_tomato_3":         (FARM_PLANTS, 48, 64, 16, 16),
    "crop_tomato_seed":      (FARM_ITEMS, 0, 64, 16, 16),
    "crop_tomato_harvested": (FARM_ITEMS, 16, 64, 16, 16),
    "crop_pumpkin_0":         (FARM_PLANTS, 0, 144, 16, 16),
    "crop_pumpkin_1":         (FARM_PLANTS, 16, 144, 16, 16),
    "crop_pumpkin_2":         (FARM_PLANTS, 32, 144, 16, 16),
    "crop_pumpkin_3":         (FARM_PLANTS, 48, 144, 16, 16),
    "crop_pumpkin_seed":      (FARM_ITEMS, 0, 144, 16, 16),
    "crop_pumpkin_harvested": (FARM_ITEMS, 16, 144, 16, 16),
    # The rest of the premium crop catalogue (every remaining 1-tile row).
    "crop_cauliflower_0": (FARM_PLANTS, 0, 48, 16, 16),
    "crop_cauliflower_1": (FARM_PLANTS, 16, 48, 16, 16),
    "crop_cauliflower_2": (FARM_PLANTS, 32, 48, 16, 16),
    "crop_cauliflower_3": (FARM_PLANTS, 48, 48, 16, 16),
    "crop_cauliflower_seed":      (FARM_ITEMS, 0, 48, 16, 16),
    "crop_cauliflower_harvested": (FARM_ITEMS, 16, 48, 16, 16),
    "crop_eggplant_0": (FARM_PLANTS, 0, 80, 16, 16),
    "crop_eggplant_1": (FARM_PLANTS, 16, 80, 16, 16),
    "crop_eggplant_2": (FARM_PLANTS, 32, 80, 16, 16),
    "crop_eggplant_3": (FARM_PLANTS, 48, 80, 16, 16),
    "crop_eggplant_seed":      (FARM_ITEMS, 0, 80, 16, 16),
    "crop_eggplant_harvested": (FARM_ITEMS, 16, 80, 16, 16),
    "crop_lettuce_0": (FARM_PLANTS, 0, 112, 16, 16),
    "crop_lettuce_1": (FARM_PLANTS, 16, 112, 16, 16),
    "crop_lettuce_2": (FARM_PLANTS, 32, 112, 16, 16),
    "crop_lettuce_3": (FARM_PLANTS, 48, 112, 16, 16),
    "crop_lettuce_seed":      (FARM_ITEMS, 0, 112, 16, 16),
    "crop_lettuce_harvested": (FARM_ITEMS, 16, 112, 16, 16),
    "crop_radish_0": (FARM_PLANTS, 0, 160, 16, 16),
    "crop_radish_1": (FARM_PLANTS, 16, 160, 16, 16),
    "crop_radish_2": (FARM_PLANTS, 32, 160, 16, 16),
    "crop_radish_3": (FARM_PLANTS, 48, 160, 16, 16),
    "crop_radish_seed":      (FARM_ITEMS, 0, 160, 16, 16),
    "crop_radish_harvested": (FARM_ITEMS, 16, 160, 16, 16),
    "crop_beetroot_0": (FARM_PLANTS, 0, 192, 16, 16),
    "crop_beetroot_1": (FARM_PLANTS, 16, 192, 16, 16),
    "crop_beetroot_2": (FARM_PLANTS, 32, 192, 16, 16),
    "crop_beetroot_3": (FARM_PLANTS, 48, 192, 16, 16),
    "crop_beetroot_seed":      (FARM_ITEMS, 0, 192, 16, 16),
    "crop_beetroot_harvested": (FARM_ITEMS, 16, 192, 16, 16),
    "crop_starfruit_0": (FARM_PLANTS, 0, 208, 16, 16),
    "crop_starfruit_1": (FARM_PLANTS, 16, 208, 16, 16),
    "crop_starfruit_2": (FARM_PLANTS, 32, 208, 16, 16),
    "crop_starfruit_3": (FARM_PLANTS, 48, 208, 16, 16),
    "crop_starfruit_seed":      (FARM_ITEMS, 0, 208, 16, 16),
    "crop_starfruit_harvested": (FARM_ITEMS, 16, 208, 16, 16),
    "crop_cucumber_0": (FARM_PLANTS, 0, 224, 16, 16),
    "crop_cucumber_1": (FARM_PLANTS, 16, 224, 16, 16),
    "crop_cucumber_2": (FARM_PLANTS, 32, 224, 16, 16),
    "crop_cucumber_3": (FARM_PLANTS, 48, 224, 16, 16),
    "crop_cucumber_seed":      (FARM_ITEMS, 0, 224, 16, 16),
    "crop_cucumber_harvested": (FARM_ITEMS, 16, 224, 16, 16),

    # Wildflowers (premium flowers sheet) - the time-rotating wild blooms.
    "flower_0": (MUSH, 16, 48, 16, 16),
    "flower_1": (MUSH, 48, 48, 16, 16),
    "flower_2": (MUSH, 96, 48, 16, 16),
    "flower_3": (MUSH, 16, 64, 16, 16),
    "flower_4": (MUSH, 96, 64, 16, 16),
    "flower_5": (MUSH, 176, 64, 16, 16),
    # Second frog color (Sorry pack frog sheet 2).
    "frog2_0": (PLANT2 / "frog/frog_spritesheet2.png", 0, 32, 16, 16),
    "frog2_1": (PLANT2 / "frog/frog_spritesheet2.png", 16, 32, 16, 16),

    # Weather icons: sunny day, clear night, rain, snow. The sheet's true
    # cell size is 32x32 (not the 16px grid every other icon sheet uses) -
    # confirmed by alpha-bbox inspection, each badge spans 2x2 nominal tiles.
    "weather_sun":  (WEATHER_ICONS, 0, 0, 32, 32),
    "weather_moon": (WEATHER_ICONS, 0, 32, 32, 32),
    "weather_rain": (WEATHER_ICONS, 96, 0, 32, 32),
    "weather_snow": (WEATHER_ICONS, 128, 0, 32, 32),

    # Corn - the 13th crop; late stages are 16x32 (it's CORN, it's tall).
    "crop_corn_0": (FARM_PLANTS, 0, 16, 16, 16),
    "crop_corn_1": (FARM_PLANTS, 16, 16, 16, 16),
    "crop_corn_2": (FARM_PLANTS, 48, 0, 16, 32),
    "crop_corn_3": (FARM_PLANTS, 64, 0, 16, 32),
    "crop_corn_seed":      (FARM_ITEMS, 0, 16, 16, 16),
    "crop_corn_harvested": (FARM_ITEMS, 16, 16, 16, 16),

    # Sunflower - the 14th crop (Sorry pack plants v2). Its band is rows
    # 15-16: row 16 holds the crop row (seeds/sprout/bud + the tall
    # stages' lower halves) and row 15 holds ONLY the bloom tops of the
    # two tall stages - row 14 above is the v2 CUCUMBER, don't touch it.
    # Items sheet row 0 pairs corn with sunflower: bag (2,0), head (3,0).
    "crop_sunflower_0": (PLANTS_V2, 32, 256, 16, 16),
    "crop_sunflower_1": (PLANTS_V2, 48, 256, 16, 16),
    "crop_sunflower_2": (PLANTS_V2, 64, 240, 16, 32),
    "crop_sunflower_3": (PLANTS_V2, 80, 240, 16, 32),
    "crop_sunflower_seed":      (ITEMS_V2, 32, 0, 16, 16),
    "crop_sunflower_harvested": (ITEMS_V2, 48, 0, 16, 16),

    # The rest of the sea life (Fish Sprites sheet) - fishing catch pools.
    "item_shrimp":    (FISH, 0, 16, 16, 16),
    "item_clownfish": (FISH, 32, 48, 16, 16),
    "item_snail":     (FISH, 48, 64, 16, 16),
    "item_crab":      (FISH, 32, 16, 16, 16),
    "item_seahorse":  (FISH, 32, 32, 16, 16),
    "item_octopus":   (FISH, 16, 16, 16, 16),
    "item_lobster":   (FISH, 16, 64, 16, 16),
    "item_ray":       (FISH, 32, 64, 16, 16),
    "item_turtle":    (FISH, 128, 0, 16, 16),

    # --- Tools ---------------------------------------------------------------
    # The pack's real tool set - Axe, Pickaxe, Hammer (flat striking face +
    # claw - the build tool), and a Watering Can (fill from water, pour
    # into a dug hole to make a pond). There is no Hoe/Shovel item -
    # tilling and hole-digging are toolless contextual actions (see
    # world_scene.cpp).
    # The Pickaxe is the dungeon pack's real crossed-head pick. The pack
    # draws NO hammer anywhere (the premium "hammer" cell is another
    # pickaxe), so the Hammer is built below in-style from the pick's own
    # palette - the license explicitly allows additional same-style
    # sprites.
    "tool_watering_can": (PREMIUM_TOOLS, 0, 0, 16, 16),
    "tool_axe":          (PREMIUM_TOOLS, 16, 0, 16, 16),
    "tool_rod":          (ROD, 0, 0, 16, 16),

    # --- Item icons ----------------------------------------------------------
    "item_wood":    (PREMIUM_TOOLS, 32, 16, 16, 16),
    "item_stone":   (PREMIUM_TOOLS, 16, 32, 16, 16),
    # Sapling ITEM icon: the stump-with-sprout (the in-world growing
    # sapling keeps the free pack's sprout, prop_sapling).
    "item_sapling": (PREMIUM_TOOLS, 16, 16, 16, 16),
    "item_egg":     (BASIC / "Objects/Egg item.png", 0, 0, 16, 16),
    "item_milk":    (BASIC / "Objects/Simple Milk and grass item.png", 0, 0, 16, 16),
    "item_hay":     (BASIC / "Objects/Simple Milk and grass item.png", 48, 0, 16, 16),
    "item_berries": (BIOM, 64, 48, 16, 16),
    # Fruit (chopping a tier-2 fruit tree drops ITS fruit - the tree art
    # and the drop are matched by the same per-tile hash), honey, fish.
    "item_apple":      (TREES, 16, 32, 16, 16),
    "item_orange":     (TREES, 48, 32, 16, 16),
    "item_pear":       (TREES, 80, 32, 16, 16),
    "item_peach":      (TREES, 112, 32, 16, 16),
    "item_honey":      (HONEY, 16, 0, 16, 16),
    "item_fish_small": (FISH, 96, 0, 16, 16),
    "item_fish_med":   (FISH, 16, 0, 16, 16),
    "item_fish_big":   (FISH, 80, 64, 16, 16),

    # --- Placeables ----------------------------------------------------------
    # (Fences draw from the premium at_fence_* line tileset - see FENCE_BLOB.)
    # Coop/Barn/Camp are windowed crops of the premium Chikcen_Houses sheet
    # (small orange / medium green / small teal), normalized below.
    # Lamps in all three colors (green/blue/pink) - A on a placed lamp
    # cycles them.
    "place_lamp":  (BASIC / "Objects/Basic Furniture.png", 48, 16, 16, 16),
    "lamp_1":      (BASIC / "Objects/Basic Furniture.png", 64, 16, 16, 16),
    "lamp_2":      (BASIC / "Objects/Basic Furniture.png", 80, 16, 16, 16),
    # Rugs: the old crop grabbed the green small rug AND most of the pink
    # one next to it. Small rugs are single 16px tiles (3 colors); the
    # sheet also has proper 32px-long rugs (3 colors) - a separate
    # placeable.
    "rug_s_0": (BASIC / "Objects/Basic Furniture.png", 0, 80, 16, 16),
    "rug_s_1": (BASIC / "Objects/Basic Furniture.png", 16, 80, 16, 16),
    "rug_s_2": (BASIC / "Objects/Basic Furniture.png", 32, 80, 16, 16),
    "rug_l_0": (BASIC / "Objects/Basic Furniture.png", 48, 80, 32, 16),
    "rug_l_1": (BASIC / "Objects/Basic Furniture.png", 80, 80, 32, 16),
    "rug_l_2": (BASIC / "Objects/Basic Furniture.png", 112, 80, 32, 16),
    # Gate leaves (closed frame of the gate animation, split into its left
    # and right doors - each is its own placeable so single-leaf gates and
    # extra-wide double gates are both buildable), blank sign, well,
    # beehive, campfire logs + separate fire frames.
    "place_gate_l":   (GATES, 0, 0, 16, 16),
    "place_gate_r":   (GATES, 16, 0, 16, 16),
    "place_sign":     (SIGNS, 0, 0, 16, 16),
    "place_well":     (WELL, 0, 0, 32, 32),
    "place_beehive":  (BEEHIVE, 0, 0, 32, 32),
    "place_campfire": (CAMPFIRE, 48, 16, 16, 16),
    "fire_0": (FIRE, 0, 0, 16, 16),
    "fire_1": (FIRE, 16, 0, 16, 16),
    "fire_2": (FIRE, 32, 0, 16, 16),
    "fire_3": (FIRE, 48, 0, 16, 16),
    # Button-prompt glyphs (Vryell's Controllers & Keyboard, Switch sheet,
    # light theme rows, bold-outline column 0) for HUD control hints.
    "ui_btn_x": (BUTTONS, 0, 16, 16, 16),
    "ui_btn_y": (BUTTONS, 0, 48, 16, 16),
    "ui_btn_b": (BUTTONS, 0, 80, 16, 16),
    "ui_btn_a": (BUTTONS, 0, 112, 16, 16),
    "ui_btn_l": (BUTTONS, 0, 432, 16, 16),
    "ui_btn_r": (BUTTONS, 0, 528, 16, 16),
    # Ambience critters: bee (circles placed beehives), frog (hops near
    # water in the wild), fish (cruise open water; med + small strips).
    "bee_0": (BEE, 0, 0, 16, 16),
    "bee_1": (BEE, 16, 0, 16, 16),
    "frog_0": (FROG, 0, 32, 16, 16),
    "frog_1": (FROG, 16, 32, 16, 16),
    "fishamb_0": (FISH_MED_STRIP, 0, 0, 16, 16),
    "fishamb_1": (FISH_MED_STRIP, 64, 0, 16, 16),
    "fishamb_s0": (FISH_SMALL_STRIP, 0, 0, 16, 16),
    "fishamb_s1": (FISH_SMALL_STRIP, 64, 0, 16, 16),

    # Animal-area furnishings. The trough's "full of hay" state is a
    # prep-time composite (see main); the tray's 3 fill states are the
    # sheet's own 32x16 cells (full / half / empty).
    "place_trough": (BARNS, 0, 0, 16, 16),
    "watertray_0": (TRAY, 0, 0, 32, 16),
    "watertray_1": (TRAY, 32, 0, 32, 16),
    "watertray_2": (TRAY, 64, 0, 32, 16),

    # Water scatter decor (hash-sprinkled on open water like lily pads):
    # two rocks, a reed clump, two more lily pad shapes.
    "wrock_0": (WATER_OBJ, 16, 0, 16, 16),
    "wrock_1": (WATER_OBJ, 48, 0, 16, 16),
    "wreed_0": (WATER_OBJ, 112, 0, 16, 16),
    "wlily_0": (WATER_OBJ, 144, 0, 16, 16),
    "wlily_1": (WATER_OBJ, 176, 0, 16, 16),

    # Fishing bobber (4 idle bob frames) + bite splash (2 frames); each is
    # a 16x32 stack (bobber cell over splash cell) from the splash sheet.
    "bob_0": (SPLASH, 0, 0, 16, 32),
    "bob_1": (SPLASH, 16, 0, 16, 32),
    "bob_2": (SPLASH, 32, 0, 16, 32),
    "bob_3": (SPLASH, 48, 0, 16, 32),
    "bite_0": (SPLASH, 64, 0, 16, 32),
    "bite_1": (SPLASH, 80, 0, 16, 32),

    # Snowlands snowfall particles (3 flake shapes).
    "snowflake_0": (SNOWFLAKES, 0, 0, 16, 16),
    "snowflake_1": (SNOWFLAKES, 0, 16, 16, 16),
    "snowflake_2": (SNOWFLAKES, 0, 32, 16, 16),

    # UI: skill-panel stars (full/half/empty) and animal-mood faces.
    "ui_star": (STARS, 0, 0, 16, 16),
    "ui_star_half": (STARS, 16, 0, 16, 16),
    "ui_star_empty": (STARS, 32, 0, 16, 16),
    "ui_happy": (HAPPY, 0, 16, 16, 16),
    "ui_sad": (HAPPY, 80, 16, 16, 16),

    # --- Animals (frame 0/1 idle, w0/w1 walking) -----------------------------
    # (Pig/fox/wolf are ORIGINAL sprites authored in-code further down -
    # Cup Nooble's packs don't ship them, and the license invites making
    # your own in the style. See emit_authored_critters().)
    "chicken_0":  (BASIC / "Characters/Free Chicken Sprites.png", 0, 0, 16, 16),
    "chicken_1":  (BASIC / "Characters/Free Chicken Sprites.png", 16, 0, 16, 16),
    "chicken_w0": (BASIC / "Characters/Free Chicken Sprites.png", 0, 16, 16, 16),
    "chicken_w1": (BASIC / "Characters/Free Chicken Sprites.png", 16, 16, 16, 16),
    "cow_0":  (BASIC / "Characters/Free Cow Sprites.png", 0, 0, 32, 32),
    "cow_1":  (BASIC / "Characters/Free Cow Sprites.png", 32, 0, 32, 32),
    "cow_w0": (BASIC / "Characters/Free Cow Sprites.png", 0, 32, 32, 32),
    "cow_w1": (BASIC / "Characters/Free Cow Sprites.png", 32, 32, 32, 32),
}

# Player walk frames: 4 rows (down/up/left/right) x 4 cols (0/1 idle bob,
# 2/3 walk) of 48x48 cells; the critter occupies the middle of each cell.
# Cropped generously then normalized bottom-center onto a 24x24 canvas so
# in-game draw math is uniform.
PLAYER_DIRS = ["down", "up", "left", "right"]
# Action poses: Basic Charakter Actions.png, 2 cols x 12 rows of 48x48.
# Rows 0-3 read as a generic 2-frame tool swing facing down/up/left/right;
# normalized onto 32x32 (tools poke out past the body).
ACTION_ROWS = {"down": 0, "up": 1, "left": 2, "right": 3}


# ---------------------------------------------------------------------------
# Procedural recolor helpers (the "difficulty tier" variants)
# ---------------------------------------------------------------------------
def hue_shift(img, degrees, hue_range=None, sat_mul=1.0, val_mul=1.0):
    """Rotate hue by `degrees`, optionally only for pixels whose current hue
    falls inside hue_range=(lo_deg, hi_deg). Alpha preserved."""
    out = img.copy()
    px = out.load()
    for yy in range(out.height):
        for xx in range(out.width):
            r, g, b, a = px[xx, yy]
            if a == 0:
                continue
            h, s, v = colorsys.rgb_to_hsv(r / 255.0, g / 255.0, b / 255.0)
            if hue_range is not None:
                hd = h * 360.0
                if not (hue_range[0] <= hd <= hue_range[1]):
                    continue
            h = (h + degrees / 360.0) % 1.0
            s = min(1.0, s * sat_mul)
            v = min(1.0, v * val_mul)
            r2, g2, b2 = colorsys.hsv_to_rgb(h, s, v)
            px[xx, yy] = (int(r2 * 255), int(g2 * 255), int(b2 * 255), a)
    return out


def tint(img, color):
    """Replace each pixel with its luminance modulated by `color` - turns
    grey rocks copper/gold while keeping the shading."""
    out = img.copy()
    px = out.load()
    for yy in range(out.height):
        for xx in range(out.width):
            r, g, b, a = px[xx, yy]
            if a == 0:
                continue
            lum = (r + g + b) / (3.0 * 255.0)
            px[xx, yy] = (int(lum * color[0]), int(lum * color[1]), int(lum * color[2]), a)
    return out


def wetten(img):
    """Watered-crop variant: darken just the tan soil-mound pixels (hue
    22-48deg, low-ish saturation, below the plant body) - the same trick
    the Sorry pack's own "farming plants v2 watered" sheet uses, applied
    to our premium-sheet crops so all 13 species get watered art."""
    out = img.copy()
    px = out.load()
    for yy in range(out.height // 2, out.height):
        for xx in range(out.width):
            r, g, b, a = px[xx, yy]
            if a == 0:
                continue
            h, s, v = colorsys.rgb_to_hsv(r / 255.0, g / 255.0, b / 255.0)
            hd = h * 360.0
            if 22.0 <= hd <= 48.0 and s <= 0.60 and v <= 0.88:
                r2, g2, b2 = colorsys.hsv_to_rgb(h, min(1.0, s * 1.15), v * 0.68)
                px[xx, yy] = (int(r2 * 255), int(g2 * 255), int(b2 * 255), a)
    return out


def darken(img, factor):
    out = img.copy()
    px = out.load()
    for yy in range(out.height):
        for xx in range(out.width):
            r, g, b, a = px[xx, yy]
            if a == 0:
                continue
            px[xx, yy] = (int(r * factor), int(g * factor), int(b * factor), a)
    return out


def make_hole(dirt_tile):
    """Dug-hole tile: mud (the dirt tile, lightly darkened) stays visible as
    a rim, with an inset dark pit drawn on top - a flat full-tile darken
    read as a plain black square with no depth."""
    out = darken(dirt_tile, 0.85)
    w, h = out.size
    draw = ImageDraw.Draw(out)
    inset = 2
    # Mid-tone rim stroke first so the pit reads as sunken, not just tinted.
    draw.ellipse((inset, inset, w - 1 - inset, h - 1 - inset),
                  fill=(40, 28, 20, 255), outline=(70, 50, 35, 255))
    inset2 = inset + 2
    draw.ellipse((inset2, inset2, w - 1 - inset2, h - 1 - inset2),
                  fill=(18, 12, 10, 255))
    return out


def normalize(img, canvas_w, canvas_h):
    """Autocrop to the alpha bounding box, then paste bottom-centered onto a
    fixed-size transparent canvas so draw anchoring is uniform in-game."""
    bbox = img.getbbox()
    if bbox:
        img = img.crop(bbox)
    canvas = Image.new("RGBA", (canvas_w, canvas_h), (0, 0, 0, 0))
    canvas.paste(img, ((canvas_w - img.width) // 2, canvas_h - img.height))
    return canvas


def make_mallet(pick):
    """Build a square-head mallet sprite from scratch, palette-sampled from
    the pick icon so it sits naturally beside the other tools."""
    px = pick.load()
    greys, browns = [], []
    dark = (40, 35, 40)
    darkest = 999
    for yy in range(pick.height):
        for xx in range(pick.width):
            r, g, b, a = px[xx, yy]
            if a == 0:
                continue
            if abs(r - g) < 24 and abs(g - b) < 24:
                greys.append((r, g, b))
            elif r > b:
                browns.append((r, g, b))
            if r + g + b < darkest:
                darkest = r + g + b
                dark = (r, g, b)
    greys.sort(key=sum)
    browns.sort(key=sum)
    steel_d = greys[len(greys) // 5]
    steel_m = greys[len(greys) // 2]
    steel_l = greys[-2]
    wood_d = browns[len(browns) // 4]
    wood_l = browns[-max(1, len(browns) // 4)]

    img = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    q = img.load()

    def put(x, y, c):
        q[x, y] = (c[0], c[1], c[2], 255)

    # Wooden handle (vertical) with outline...
    for yy in range(4, 15):
        put(6, yy, dark)
        put(7, yy, wood_l)
        put(8, yy, wood_d)
        put(9, yy, dark)
    put(7, 15, dark)
    put(8, 15, dark)
    # ...capped by a wide steel head drawn over the top.
    for xx in range(3, 13):
        put(xx, 0, dark)
        put(xx, 1, steel_l)
        put(xx, 2, steel_m)
        put(xx, 3, steel_m)
        put(xx, 4, steel_d)
        put(xx, 5, dark)
    for yy in range(0, 6):
        put(2, yy, dark)
        put(13, yy, dark)
    return img


def keep_largest(img):
    """Erase everything except the largest connected alpha component - for
    windowed crops where a neighboring sprite's edge pixels sneak in."""
    px = img.load()
    w, h = img.size
    seen = [[False] * w for _ in range(h)]
    best = []
    for yy in range(h):
        for xx in range(w):
            if seen[yy][xx] or px[xx, yy][3] == 0:
                continue
            comp = []
            stack = [(xx, yy)]
            seen[yy][xx] = True
            while stack:
                cx, cy = stack.pop()
                comp.append((cx, cy))
                for dx in (-1, 0, 1):
                    for dy in (-1, 0, 1):
                        nx, ny = cx + dx, cy + dy
                        if 0 <= nx < w and 0 <= ny < h and not seen[ny][nx] and px[nx, ny][3] != 0:
                            seen[ny][nx] = True
                            stack.append((nx, ny))
            if len(comp) > len(best):
                best = comp
    keep = set(best)
    for yy in range(h):
        for xx in range(w):
            if px[xx, yy][3] != 0 and (xx, yy) not in keep:
                px[xx, yy] = (0, 0, 0, 0)
    return img


# ---------------------------------------------------------------------------
# Original critters: pig, fox, wolf (and the piglet). Cup Nooble's packs
# ship only chickens and cows; these are hand-authored pixel grids in the
# Sprout Lands style - the pack's #504086 outline, its pink/orange/grey
# ramps sampled from real sprites, 16x16 left-facing bodies with the same
# 2-frame idle bob + 2-frame leg-swap walk the pack animals use. The
# license explicitly invites "making your own sprites in this style".
CRITTER_OUTLINE = (0x50, 0x40, 0x86, 255)
CRITTER_PALS = {
    "pig": {"O": CRITTER_OUTLINE, "P": (0xE8, 0xB5, 0xAC, 255),
            "L": (0xF3, 0xD8, 0xC5, 255), "D": (0xC5, 0x80, 0x86, 255)},
    "fox": {"O": CRITTER_OUTLINE, "F": (0xEE, 0xBA, 0x77, 255),
            "R": (0xBA, 0x7C, 0x54, 255), "W": (0xF3, 0xF2, 0xC0, 255)},
    "wolf": {"O": CRITTER_OUTLINE, "G": (0x9D, 0xA8, 0x9A, 255),
             "S": (0x6B, 0x74, 0x70, 255), "W": (0xC1, 0xC8, 0xB9, 255)},
    "alpaca": {"O": CRITTER_OUTLINE, "W": (0xF3, 0xF2, 0xC0, 255),
               "T": (0xE8, 0xCF, 0xA6, 255), "D": (0xC4, 0x9A, 0x6C, 255)},
    "cat": {"O": CRITTER_OUTLINE, "C": (0xEE, 0xBA, 0x77, 255),
            "W": (0xF3, 0xF2, 0xC0, 255), "D": (0xBA, 0x7C, 0x54, 255)},
    "shroomling": {"O": CRITTER_OUTLINE, "M": (0xC5, 0x80, 0x86, 255),
                   "L": (0xF3, 0xD8, 0xC5, 255), "W": (0xF3, 0xF2, 0xC0, 255)},
}
CRITTER_GRIDS = {
    "pig": [
        "................", "................", "................",
        "...OO...........",
        "..OPPO.OOOOOO...",
        "..OPPPOLLLLLLO..",
        ".OPPPLLLPPPPPPO.",
        "ODDPPOPPPPPPPPO.",
        "ODDPPPPPPPPPPPOO",
        "OODPPPPPPPPPPPDO",
        ".OPPPPPPPPPPPOO.",
        ".ODPPPPPPPPPDO..",
        "..OPPOOOOPPOO...",
        "..ODPO..ODPO....",
        "...OO....OO.....",
        "................",
    ],
    "piglet": [
        "................", "................", "................",
        "................", "................", "................",
        "................", "................",
        "....OO..........",
        "...OPPO.OOOO....",
        "..ODDPPOLLPPO...",
        "..ODPPPPPPPPO...",
        "...OPPPPPPPDO...",
        "...OPOO..OPO....",
        "....O.....O.....",
        "................",
    ],
    "fox": [
        "................", "................", "................",
        ".OO...OO........",
        ".ORO.ORFO.......",
        ".OFFOOFFFO......",
        ".OFFFFFFFFO.....",
        "OOFOFFFFFFOOOO..",
        "OWFFFFFFFFOFFWO.",
        ".OWWFFFFFFOFWWO.",
        ".OWFFFFFFFFOWO..",
        "..OFFFFFFFOOO...",
        "..OFFOOOFFO.....",
        "..OFRO.OFRO.....",
        "...OO...OO......",
        "................",
    ],
    "wolf": [
        "................", "................",
        ".OO....OO.......",
        ".OSGO.OGSO......",
        ".OGGGOOGGGO.....",
        ".OGGGGGGGGO.....",
        "OOGGOGGGGGGO....",
        "OWGGGGGGGGGOO...",
        "OWWGGGGGGGGGOSO.",
        ".OWGGGGGGGGGGSO.",
        "..OGGGGGGGGGOO..",
        "..OGGGGGGGGO....",
        "..OGGOOOOGGO....",
        "..OGSO..OGSO....",
        "...OO....OO.....",
        "................",
    ],
}
CRITTER_GRIDS["alpaca"] = [
    "................",
    "..OO.O..........",
    ".OWWOWO.........",
    ".OWWWWO.........",
    ".OWOWWO.........",
    "..OWWWO.........",
    "..OWWWO.........",
    "..OWWWWOOOOO....",
    ".OWWWWWWWWWWWO..",
    ".OWWWWWWWWWWWWO.",
    ".ODWWWWWWWWWWDO.",
    "..OWWWWWWWWWWO..",
    "..OWTOOOOWTOO...",
    "..OTTO..OTTO....",
    "...OO....OO.....",
    "................",
]
CRITTER_GRIDS["shroomling"] = [
    "................", "................", "................",
    "................", "................",
    "...OOOOOO.......",
    "..OMMMMMMO......",
    ".OMLMMLMMMO.....",
    ".OMMMMMMMMO.....",
    "..OOOOOOOO......",
    "...OWWWWO.......",
    "...OWOOWO.......",
    "...OWWWWO.......",
    "...OWO.OWO......",
    "....O...O.......",
    "................",
]
CRITTER_GRIDS["cat"] = [
    "................", "................", "................",
    "................", "................",
    ".OO..OO.........",
    ".OCOOCO.........",
    ".OCCCCCO....O...",
    "OCOCCCCO...OCO..",
    "OCCCCCCO..OCO...",
    ".OCCCCCCOOCO....",
    ".OWCCCCCCCO.....",
    "..OCCCCCCO......",
    "..OCOO.OCO......",
    "..OO...OO.......",
    "................",
]
# Rows that are LEGS (stay planted during the idle bob, swap during the
# walk). Everything above them is the body.
CRITTER_LEG_ROWS = {"pig": 12, "piglet": 13, "fox": 12, "wolf": 12,
                    "alpaca": 12, "cat": 13, "shroomling": 13}


def render_critter_grid(grid, pal):
    img = Image.new("RGBA", (16, 16))
    px = img.load()
    for y, row_s in enumerate(grid):
        for x, ch in enumerate(row_s):
            if ch != ".":
                px[x, y] = pal[ch]
    return img


def critter_frames(grid, pal, leg_row):
    """(idle0, idle1, walk0, walk1): idle bob shifts the body 1px down over
    planted legs; the walk swaps the leg halves 1px toward each other."""
    base = render_critter_grid(grid, pal)
    body = base.crop((0, 0, 16, leg_row))
    legs = base.crop((0, leg_row, 16, 16))
    idle0 = base
    idle1 = Image.new("RGBA", (16, 16))
    idle1.paste(body, (0, 1))
    idle1.paste(legs, (0, leg_row), legs)
    left = legs.crop((0, 0, 8, 16 - leg_row))
    right = legs.crop((8, 0, 16, 16 - leg_row))
    walk0 = Image.new("RGBA", (16, 16))
    walk0.paste(body, (0, 0))
    walk0.paste(left, (1, leg_row), left)
    walk0.paste(right, (7, leg_row), right)
    walk1 = Image.new("RGBA", (16, 16))
    walk1.paste(body, (0, 0))
    walk1.paste(left, (0, leg_row + 1), left)
    walk1.paste(right, (8, leg_row + 1), right)
    # The grids are authored facing left (easier to read in text), but the
    # game's wild-animal pass assumes right-facing art and mirrors it when
    # a critter heads left - so flip everything to match the pack.
    return tuple(f.transpose(Image.FLIP_LEFT_RIGHT) for f in (idle0, idle1, walk0, walk1))


def main():
    GFX_DIR.mkdir(exist_ok=True)
    # Wipe stale slices so renamed/removed sprites don't linger in the atlas.
    for old in GFX_DIR.glob("*.png"):
        old.unlink()

    sheets = {}

    def load(path):
        if path not in sheets:
            img = Image.open(path).convert("RGBA")
            img.load()
            sheets[path] = img
        return sheets[path]

    generated = {}

    def emit(name, img):
        generated[name] = img
        img.save(GFX_DIR / f"{name}.png")

    for name, (sheet, x, y, w, h) in SPRITES.items():
        emit(name, load(sheet).crop((x, y, x + w, y + h)))

    # Original critters (CRITTER_GRIDS above): the pig in five colors like
    # the pack animals, its piglet baby, and the wild fox/wolf pair (red +
    # arctic fox for the Snowlands, grey + black wolf).
    def emit_critter(prefix, grid_key, pal_key, recolor=None):
        i0, i1, w0, w1 = critter_frames(CRITTER_GRIDS[grid_key], CRITTER_PALS[pal_key],
                                        CRITTER_LEG_ROWS[grid_key])
        for suffix, img in (("_0", i0), ("_1", i1), ("_w0", w0), ("_w1", w1)):
            emit(prefix + suffix, recolor(img) if recolor else img)

    PIG_COLORS = [None,
                  lambda c: hue_shift(c, 25, None, 0.80, 0.88),   # rosy brown
                  lambda c: darken(c, 0.62),                      # sooty
                  lambda c: hue_shift(c, -30, None, 0.45, 1.08),  # cream
                  lambda c: hue_shift(c, 150, None, 0.30, 0.92)]  # lilac-grey
    for i, rc in enumerate(PIG_COLORS):
        emit_critter("pig" if i == 0 else f"pig{i}", "pig", "pig", rc)
        emit_critter(f"piglet{i}", "piglet", "pig", rc)
    emit_critter("fox", "fox", "fox")
    emit_critter("fox_snow", "fox", "fox", lambda c: hue_shift(c, 0, None, 0.15, 1.22))
    emit_critter("wolf", "wolf", "wolf")
    emit_critter("wolf_black", "wolf", "wolf", lambda c: darken(c, 0.55))
    emit_critter("alpaca", "alpaca", "alpaca")
    emit_critter("alpaca1", "alpaca", "alpaca", lambda c: hue_shift(c, 18, None, 0.85, 0.82))
    emit_critter("shroomling", "shroomling", "shroomling")
    emit_critter("shroomling1", "shroomling", "shroomling",
                 lambda c: hue_shift(c, -60, None, 1.1, 0.9))  # dusk-purple cap
    for i, rc in enumerate([None,
                            lambda c: hue_shift(c, 0, None, 0.18, 0.95),  # grey tabby
                            lambda c: darken(c, 0.52)]):                  # black cat
        emit_critter("cat" if i == 0 else f"cat{i}", "cat", "cat", rc)

    # --- Autotile sets --------------------------------------------------------
    # Nearly every Sprout Lands ground tileset shares one "standard blob"
    # arrangement (see the pack's own Bitmask references sheets): a 3x3
    # rounded blob at cols 0-2 / rows 0-2, a 1-wide vertical strip in col 3
    # (top/mid/bottom caps), a 1-tall horizontal strip in row 3 (left/mid/
    # right caps), and a lone island tile at (3,3). Those 16 cells map
    # exactly onto a 4-bit cardinal neighbor mask (N=1 E=2 S=4 W=8, bit set
    # = same-material neighbor), which is what the game computes at draw
    # time - so paths turn corners, tilled beds get rounded edges, the
    # snow line gets a real border, and mine floors look carved.
    STD_BLOB = {
        0: (3, 3),                                # island (no neighbors)
        1: (3, 2), 2: (0, 3), 4: (3, 0), 8: (2, 3),   # strip caps
        5: (3, 1), 10: (1, 3),                    # strip middles
        3: (0, 2), 9: (2, 2), 6: (0, 0), 12: (2, 0),  # blob corners
        7: (0, 1), 13: (2, 1), 14: (1, 0), 11: (1, 2),# blob edges
        15: (1, 1),                               # interior
    }

    def emit_autotile(prefix, img, recolor=None):
        for mask, (c, r) in STD_BLOB.items():
            cell = img.crop((c * 16, r * 16, c * 16 + 16, r * 16 + 16))
            emit(f"{prefix}_{mask}", recolor(cell) if recolor else cell)
        # Inner corners: a tile whose cardinals all match but a DIAGONAL
        # doesn't (path crossings, wall notches) needs a rounded notch at
        # that corner, not a square interior edge. The sheets ship this as
        # the "blob with holes" group (cols 4-7): each hole sits on a
        # 4-cell junction, so its quarters are the per-corner notch
        # patches. Emit all 15 notch combinations composited onto the
        # interior tile; bit order NW=1 NE=2 SW=4 SE=8. Junction used:
        # between cols 5-6 / rows 1-2 (central, clean on every sheet).
        interior = img.crop((16, 16, 32, 32))
        quads = [
            img.crop((96, 32, 104, 40)),  # NW notch: SE-of-junction cell, top-left
            img.crop((88, 32, 96, 40)),   # NE notch: SW cell, top-right
            img.crop((96, 24, 104, 32)),  # SW notch: NE cell, bottom-left
            img.crop((88, 24, 96, 32)),   # SE notch: NW cell, bottom-right
        ]
        for nm in range(1, 16):
            t = interior.copy()
            if nm & 1:
                t.paste(quads[0], (0, 0))
            if nm & 2:
                t.paste(quads[1], (8, 0))
            if nm & 4:
                t.paste(quads[2], (0, 8))
            if nm & 8:
                t.paste(quads[3], (8, 8))
            emit(f"{prefix}_n{nm}", recolor(t) if recolor else t)

    emit_autotile("at_grass", load(BASIC / "Tilesets/Grass.png"))
    # Raw dirt (filled holes, un-tilled beds, dug-up paths). The basic
    # pack's "Tilled Dirt.png" authors its dirt blob as OVERLAYS: only
    # the blob's center cell (2,4) is a full dirt tile - the 8 cells
    # around it are bleed skirts meant to be composited onto the
    # NEIGHBORING grass tiles (a 5-6px rounded dirt lip hanging over the
    # shared edge; every cell's alpha bbox confirms it). So the game
    # draws dirt tiles flat (tile_dirt plus the speckled variants below)
    # and grass tiles bordering dirt add the matching skirt pieces - see
    # the dirt-skirt pass in world_scene.cpp. Pieces are named for WHERE
    # THE DIRT NEIGHBOR IS relative to the grass tile that draws them.
    dirt_sheet = load(BASIC / "Tilesets/Tilled Dirt.png")
    DIRT_PIECES = {
        "dirt_skirt_n": (2, 5),  # dirt north of me: lip along my top edge
        "dirt_skirt_s": (2, 3),  # dirt south: lip along my bottom edge
        "dirt_skirt_e": (1, 4),  # dirt east: lip along my right edge
        "dirt_skirt_w": (3, 4),  # dirt west: lip along my left edge
        "dirt_nub_ne": (1, 5),   # diagonal-only bleed: one rounded nub
        "dirt_nub_nw": (3, 5),
        "dirt_nub_se": (1, 3),
        "dirt_nub_sw": (3, 3),
        # Speckled flat variants, mixed in by visHash like the grass flats.
        "dirt_v1": (1, 0),
        "dirt_v2": (2, 0),
        "dirt_v3": (1, 1),
        "dirt_v4": (2, 1),
    }
    for name, (c, r) in DIRT_PIECES.items():
        emit(name, dirt_sheet.crop((c * 16, r * 16, c * 16 + 16, r * 16 + 16)))
    emit_autotile("at_path", load(SOIL))
    # Stone path gets a desaturated grey take on the same blob so the path
    # family reads distinctly: dirt (brown), stone (grey + stone scatter),
    # plank (brown + laid planks).
    emit_autotile("at_pstone", load(SOIL), lambda c: hue_shift(c, 0, None, 0.15, 1.02))
    # Tilled beds: the same soil blob, darkened to read as worked ground
    # (and darker again when watered) - matching the flat tilled tiles,
    # which are rebased onto this sheet's interior cell below.
    emit_autotile("at_till", load(SOIL), lambda c: darken(c, 0.85))
    # (No separate wet set: watered soil is the same art, tinted darker at
    # draw time, so wet/dry seams share every edge and notch pixel.)
    emit_autotile("at_snow", load(SNOW1))
    emit_autotile("at_mine", load(D_GROUND))
    # Deep-mine floors: the pack's own darker recolor, same blob layout.
    emit_autotile("at_mined", load(D_GROUND_DARK))
    # Hedge-maze walls: the premium bush blob, full edge + notch sets.
    emit_autotile("at_hedge", load(BUSH_TILES))
    # Hill plateaus (elevation rims; interiors are plain grass so only the
    # edge masks ever draw) + the stair ramp overlay for south rims. The
    # slopes sheet's rightmost group is the full standalone stairway (the
    # others have cliff-rock outcrops baked in); it's painted on opaque
    # grass, so key out the green to leave just the steps.
    emit_autotile("at_hill", load(HILLS))
    # The ramp motif: one 32px stair unit between the jamb-rock centers
    # (rocks sit ON the unit boundaries at x=32/x=64 and are shared with
    # the neighboring cliff runs, so the opening shows only each rock's
    # inward-facing ARC, not whole rocks). Key out only the BRIGHT grass
    # background - the rocks' dark-green outlines stay so they read
    # grounded.
    steps = load(SLOPES).crop((32, 0, 64, 48))
    spx = steps.load()
    for yy in range(steps.height):
        for xx in range(steps.width):
            r, g, b, a = spx[xx, yy]
            if a and g > r and g > 150:
                spx[xx, yy] = (0, 0, 0, 0)
    emit("hill_steps", steps)

    # Cave walls (dungeon walls sheet, boulder-wall kit at rows 12-16):
    # a wall-POV autotile - boulder rims on whichever sides FACE open
    # floor, dark rock core inside thick walls, and a junction-knob group
    # for cores whose only floor contact is diagonal. Mask bits: floor at
    # N=1 E=2 S=4 W=8 (the inverse of the ground blobs' semantics).
    dwalls = load(D_WALLS)

    def dwcell(cx, cy):
        return dwalls.crop((cx * 16, 192 + cy * 16, cx * 16 + 16, 192 + cy * 16 + 16))

    # Kit layout (region rows 0-5 = sheet rows 12-17): row 0 ring-top
    # rim, row 1 sides + plain core, row 2 ring-bottom rim + strip S-cap,
    # row 3 horizontal strip (slit), row 4 SOLID BOULDER FACE, row 5 the
    # decorated wall-top ring (pebbled variants - unused here). South-
    # facing pieces use the row-4 faces so a north wall reads two tiles
    # tall: rim cap above, boulder face below, like the pack's promo.
    # North walls are TWO tiles tall everywhere: the row above a face
    # draws its rim caps toward it (the "ridge rule" in drawMine), and
    # the face row itself is the solid boulder fill.
    DWALL_MAP = {
        0: (1, 1),                                    # dark core
        1: (1, 0), 2: (2, 1), 4: (1, 4), 8: (0, 1),   # rims + solid face
        3: (2, 0), 9: (0, 0), 6: (2, 4), 12: (0, 4),  # corners (lone-mass row)
        5: (1, 3), 10: (3, 1),                        # opposite-side strips
        # 7/13 (floor N+E+S / N+S+W - the ends of a 1-tall wall run): read
        # as the face with the rim toward the open end, same cells as
        # masks 6/12. The rounded end-cap art moved to its own dcap_e
        # sprite for cap-row / pillar-cap use.
        7: (2, 4), 13: (0, 4), 11: (3, 0), 14: (3, 2),# strip end caps
        15: (3, 4),                                   # lone pillar (own cell,
    }                                                  # own cap piece above - see drawMine)
    # The cap row: sits directly above a face, crowning it with rim caps so
    # every north wall reads two tiles tall (dtop_s = plain cap; _se/_sw
    # are the cap-row's own corner turns). Distinct from the plain
    # mask6/12 corners above, which moved to the lone-mass row - the two
    # situations need different art (a cap-row corner isn't a floor corner).
    emit("dtop_s", dwcell(1, 2))
    # Cap-row run ends per the hand-drawn grid spec: floor E of the cap ->
    # the (5,15) jamb piece; floor W of the cap -> the ring-bottom (0,14).
    # Asymmetric on the sheet - that's the pack's own design.
    emit("dtop_se", dwcell(5, 3))
    emit("dtop_sw", dwcell(0, 2))
    # The mirror jamb (6,15): the S+W cap where a vertical wall meets a
    # walk-behind run extending to its lower-left - the true mirror of the
    # S+E jamb (5,15).
    emit("dtop_sw_j", dwcell(6, 3))
    # Walk-behind ridge strip over a 1-tall wall run (floor above AND
    # below). Overlaid on the FLOOR tile north of the wall; its black top
    # band is translucent so the player walking there reads as passing
    # behind the ridge. Three pieces autotile by open side: open-left =
    # left end (0,15), walls both sides = middle (1,15), open-right =
    # right end (2,15).
    emit("dcap_l", dwcell(0, 3))
    emit("dcap_m", dwcell(1, 3))
    emit("dcap_r", dwcell(2, 3))
    # Boulder face variants (row 19): plain + sprout/moss + pocked. A
    # hashed pick keeps long face runs from looking tiled.
    emit("dface0", dwcell(0, 7))
    emit("dface1", dwcell(1, 7))
    emit("dface2", dwcell(2, 7))
    emit("dface3", dwcell(3, 7))
    emit("dface4", dwcell(4, 7))
    # Face-junction pieces: a 2-tall wall face where a 1-tall walk-behind
    # run butts into its side. Run on the EAST -> 5,16 (left outer wall);
    # run on the WEST -> 6,16 (right outer wall).
    emit("dface_cl", dwcell(5, 4))
    emit("dface_cr", dwcell(6, 4))
    # r2's WIDE-notch variant: (5,15) is only right for a 1-wide notch
    # (paired with the walk-behind nub below); a 2+-wide notch needs this
    # piece instead - confirmed live in the lab.
    emit("dtop_se_w", dwcell(2, 2))
    # The rounded end-caps (old mask-7/13 art): still the right pieces for
    # cap-row cells over nub tips and for the lone pillar's crown.
    emit("dcap_e", dwcell(3, 3))
    emit("dcap_w", dwcell(0, 3))
    # Round-4 walk-behind turn pieces (named + verified in the wall lab; the
    # user's corrections asked for these five tiles no sprite covered yet):
    # rim/cap variants leaning into a perpendicular run at an L-turn, and the
    # both-sides face return where 1-tall runs butt a wall from BOTH sides.
    emit("drim_e2", dwcell(4, 0))
    emit("drim_w2", dwcell(7, 0))
    emit("dcap_e2", dwcell(4, 3))
    emit("dcap_w2", dwcell(7, 3))
    emit("dface_cb", dwcell(8, 4))
    for m, (cx, cy) in DWALL_MAP.items():
        emit(f"at_dwall_{m}", dwcell(cx, cy))
    # Grey-brick built-room kit (rows 2-9 of the same sheet - identical
    # wall-POV layout to the boulder kit: brown wall-top ringed by stone
    # caps, brick FACE on south-facing runs). Face variants: cracked,
    # barred window, mossy, and the walkable arch doorway. Plus the r0
    # bone/skull decals, whose background IS the wall-top brown - they
    # overlay seamlessly as core texture.
    # Same layout one band up: row 2 ring-top, row 3 sides/core, row 4
    # ring-bottom, row 5 the all-caps ridge strip, row 6 the brick face.
    BWALL_MAP = {
        0: (1, 3),
        1: (1, 2), 2: (2, 3), 4: (1, 6), 8: (0, 3),
        3: (2, 2), 9: (0, 2), 6: (2, 6), 12: (0, 6),  # corners (lone-mass row)
        5: (1, 5), 10: (3, 3),
        7: (2, 6), 13: (0, 6), 11: (3, 2), 14: (3, 4),
        15: (3, 6),                                   # lone pillar (own cell)
    }
    for m, (cx, cy) in BWALL_MAP.items():
        emit(f"at_bwall_{m}", dwalls.crop((cx * 16, cy * 16, cx * 16 + 16, cy * 16 + 16)))
    # Cap row (see the boulder dtop_s/_se/_sw comment above - identical
    # layout, one band up).
    emit("btop_s", dwalls.crop((1 * 16, 4 * 16, 2 * 16, 5 * 16)))
    emit("btop_se", dwalls.crop((5 * 16, 5 * 16, 6 * 16, 6 * 16)))
    emit("btop_sw", dwalls.crop((0 * 16, 4 * 16, 1 * 16, 5 * 16)))
    emit("btop_sw_j", dwalls.crop((6 * 16, 5 * 16, 7 * 16, 6 * 16)))
    emit("bcap_e", dwalls.crop((3 * 16, 5 * 16, 4 * 16, 6 * 16)))
    emit("bcap_w", dwalls.crop((0 * 16, 5 * 16, 1 * 16, 6 * 16)))
    # Walk-behind ridge strip over a 1-tall wall run: left end / middle /
    # right end. Overlaid on the FLOOR tile north of the wall; picked by
    # which sides are open (open-left -> left end, open-right -> right end,
    # walls both sides -> middle). Brick mirror of the boulder row.
    emit("bcap_l", dwalls.crop((0 * 16, 5 * 16, 1 * 16, 6 * 16)))
    emit("bcap_m", dwalls.crop((1 * 16, 5 * 16, 2 * 16, 6 * 16)))
    emit("bcap_r", dwalls.crop((2 * 16, 5 * 16, 3 * 16, 6 * 16)))
    emit("btop_se_w", dwalls.crop((2 * 16, 4 * 16, 3 * 16, 5 * 16)))
    # Face-junction pieces (see dface_cl/_cr) - one band up.
    emit("bface_cl", dwalls.crop((5 * 16, 6 * 16, 6 * 16, 7 * 16)))
    emit("bface_cr", dwalls.crop((6 * 16, 6 * 16, 7 * 16, 7 * 16)))
    # Corner-cap "with a wall" variant (see the boulder dtop_sw2/_se2
    # comment - identical layout, one band up).
    emit("btop_sw2", dwalls.crop((7 * 16, 3 * 16, 8 * 16, 4 * 16)))
    emit("btop_se2", dwalls.crop((4 * 16, 3 * 16, 5 * 16, 4 * 16)))
    # Round-4 walk-behind turn pieces - same cells as the boulder kit's
    # drim_e2/_w2, dcap_e2/_w2, dface_cb, ten rows up.
    emit("brim_e2", dwalls.crop((4 * 16, 2 * 16, 5 * 16, 3 * 16)))
    emit("brim_w2", dwalls.crop((7 * 16, 2 * 16, 8 * 16, 3 * 16)))
    emit("bcap_e2", dwalls.crop((4 * 16, 5 * 16, 5 * 16, 6 * 16)))
    emit("bcap_w2", dwalls.crop((7 * 16, 5 * 16, 8 * 16, 6 * 16)))
    emit("bface_cb", dwalls.crop((8 * 16, 6 * 16, 9 * 16, 7 * 16)))
    for nm2, (cx, cy) in {"bface_crack": (3, 6), "bface_window": (5, 9),
                          "bface_moss": (9, 9), "bface_arch": (7, 9),
                          "bface_arch2": (8, 9),  # lantern-topped doorway
                          "decal_bone": (5, 0), "decal_bone2": (7, 0),
                          "decal_skull": (9, 0),
                          # Sparkle/pebble wall-top decor (same brown bg).
                          "decal_spark": (4, 1), "decal_spark2": (6, 1),
                          "decal_pebbles": (1, 1)}.items():
        emit(nm2, dwalls.crop((cx * 16, cy * 16, cx * 16 + 16, cy * 16 + 16)))
    # Brick corner tiles (whole 16x16 cells, brick knob group's central
    # junction, cols 5-6 / rows 3-4) - same treatment as dcorner_* above.
    emit("bcorner_nw", dwalls.crop((6 * 16, 4 * 16, 7 * 16, 5 * 16)))
    emit("bcorner_ne", dwalls.crop((5 * 16, 4 * 16, 6 * 16, 5 * 16)))
    emit("bcorner_sw", dwalls.crop((6 * 16, 3 * 16, 7 * 16, 4 * 16)))
    emit("bcorner_se", dwalls.crop((5 * 16, 3 * 16, 6 * 16, 4 * 16)))
    # Double corners (both diagonals on one edge) - see dcorner_n2/_s2.
    emit("bcorner_n2", dwalls.crop((8 * 16, 4 * 16, 9 * 16, 5 * 16)))
    emit("bcorner_s2", dwalls.crop((8 * 16, 3 * 16, 9 * 16, 4 * 16)))
    # "Corner with the opposite wall" (see dcorner_nw2/_ne2) - confirmed at
    # col 7 row 4 by direct report.
    emit("bcorner_nw2", dwalls.crop((7 * 16, 4 * 16, 8 * 16, 5 * 16)))
    emit("bcorner_ne2", dwalls.crop((4 * 16, 4 * 16, 5 * 16, 5 * 16)))

    # Corner tiles (whole 16x16 cells, from the knob group's central
    # junction): a dedicated tile picked whenever floor meets a wall cell
    # only at one diagonal (both orthogonal neighbors on that corner still
    # wall) - never composited over another piece, one tile in, one out.
    emit("dcorner_nw", dwcell(6, 2))
    emit("dcorner_ne", dwcell(5, 2))
    emit("dcorner_sw", dwcell(6, 1))
    emit("dcorner_se", dwcell(5, 1))
    # Double corners: floor touches BOTH diagonals on one edge (two top /
    # two bottom corners at once) - a dedicated piece at col 8, not two
    # single-corner tiles fighting over the cell.
    emit("dcorner_n2", dwcell(8, 2))
    emit("dcorner_s2", dwcell(8, 1))
    # "Corner with the opposite wall": knw/kne with an extra floor opening
    # elsewhere on this row (same idea as the ccnw2/ccne2 cap variants,
    # confirmed at col 7 one row down from knw). ksw/kse's own variant IS
    # ccnw2/ccne2 - they already share a cell with those, no new sprite.
    emit("dcorner_nw2", dwcell(7, 2))
    emit("dcorner_ne2", dwcell(4, 2))
    # Corner-cap "with a wall": when the cap-row cell curving toward a face
    # ALSO has its own floor opening on the opposite side (not just a bare
    # stub), the plain curve piece is wrong - the sheet has a dedicated
    # variant one column over from the plain corner.
    emit("dtop_sw2", dwcell(7, 1))
    emit("dtop_se2", dwcell(4, 1))

    # Premium fence set - unlike the ground blobs this is a LINE tileset
    # with true junction pieces: 4 corners, 4 T-pieces, and a 4-way cross
    # (mask semantics match: bit set = fence continues that way). Layout:
    # vertical strip in col 0 (top/mid/bottom, lone post at (0,3)), the
    # 3x3 junction block in cols 1-3, horizontal strip along row 3.
    FENCE_BLOB = {
        0: (0, 3),                                 # lone post
        4: (0, 0), 5: (0, 1), 1: (0, 2),           # vertical: top cap, mid, bottom cap
        2: (1, 3), 10: (2, 3), 8: (3, 3),          # horizontal: left cap, mid, right cap
        6: (1, 0), 14: (2, 0), 12: (3, 0),         # NW corner, T-down, NE corner
        7: (1, 1), 15: (2, 1), 13: (3, 1),         # T-right, 4-way cross, T-left
        3: (1, 2), 11: (2, 2), 9: (3, 2),          # SW corner, T-up, SE corner
    }
    pfence = load(P_FENCES)
    for mask, (c, r) in FENCE_BLOB.items():
        emit(f"at_fence_{mask}", pfence.crop((c * 16, r * 16, c * 16 + 16, r * 16 + 16)))

    # Interior wall kit: the walls tilesheet's left 3x3 group is a
    # complete 9-slice room - dark-plank band along the north, thin frame
    # strips over the brick floor on the sides, light-plank band with
    # rounded outer corners along the south. Building interiors compose
    # their wall ring from these instead of the tall exterior facade.
    for iwname, (icx, icy) in {"iw_nw": (0, 0), "iw_n": (1, 0), "iw_ne": (2, 0),
                               "iw_w": (0, 1), "iw_e": (2, 1),
                               "iw_sw": (0, 2), "iw_s": (1, 2), "iw_se": (2, 2)}.items():
        emit(iwname, load(WALLS).crop((icx * 16, icy * 16, icx * 16 + 16, icy * 16 + 16)))
    # Bare 16px door leaves (no facade band) for the interior south wall.
    emit("door_leaf", load(DOORS).crop((0, 16, 16, 32)))
    emit("door_leaf_open", load(DOORS).crop((0, 0, 16, 16)))

    # The plain wall piece: facade top band over clean plank rows. (The
    # sheet's panel wall is 2 columns wide with the inset panel split
    # between them - either column alone reads lopsided, which is what
    # the old single-crop place_wall looked like.)
    wall = Image.new("RGBA", (16, 32), (0, 0, 0, 0))
    wall.paste(load(WALLS).crop((16, 0, 32, 16)), (0, 0))
    wall.paste(load(WALLS).crop((48, 16, 64, 32)), (0, 16))
    emit("place_wall", wall)

    # Wooden floor blob: no pack ships one, so it's built procedurally -
    # the soil blob's rounded alpha silhouette filled with the interior
    # brick-floor texture (walls tilesheet center), plus a 2px darkened
    # rim along the rounded edge so floors read as raised. Out-of-cell
    # neighbors count as solid so tile boundaries stay seamless.
    plank = load(WALLS).crop((16, 16, 32, 32)).load()
    soil_sheet = load(SOIL)
    for mask, (c, r) in STD_BLOB.items():
        sil = soil_sheet.crop((c * 16, r * 16, c * 16 + 16, r * 16 + 16)).load()

        def solid(x, y):
            if x < 0 or y < 0 or x > 15 or y > 15:
                return True
            return sil[x, y][3] > 0

        cell = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
        px = cell.load()
        for y in range(16):
            for x in range(16):
                if not solid(x, y):
                    continue
                pr, pg, pb, pa = plank[x, y]
                near = [solid(x + dx, y + dy) for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1))]
                far = [solid(x + dx, y + dy)
                       for dx, dy in ((-2, 0), (2, 0), (0, -2), (0, 2),
                                      (-1, -1), (1, -1), (-1, 1), (1, 1))]
                if not all(near):
                    f = 0.45  # outermost rim: strong shadow line
                elif not all(far):
                    f = 0.72  # second band: soft falloff
                else:
                    f = 1.0
                px[x, y] = (int(pr * f), int(pg * f), int(pb * f), pa)
        emit(f"at_floor_{mask}", cell)

    # Player walk frames + action poses.
    char = load(CHAR)
    for row, d in enumerate(PLAYER_DIRS):
        for frame in range(4):
            cell = char.crop((frame * 48 + 8, row * 48 + 8, frame * 48 + 40, row * 48 + 40))
            emit(f"player_{d}_{frame}", normalize(cell, 24, 24))
    # The Actions sheet is 3 tools x 4 directions: rows 0-3 = hoe/dig,
    # rows 4-7 = axe swing, rows 8-11 = watering. Each in-game action
    # picks the pose set matching the tool that triggered it.
    actions = load(ACTIONS)
    for prefix, base in [("act", 0), ("act_axe", 4), ("act_water", 8)]:
        for d, row in ACTION_ROWS.items():
            for frame in range(2):
                cell = actions.crop((frame * 48, (base + row) * 48,
                                     frame * 48 + 48, (base + row) * 48 + 48))
                emit(f"{prefix}_{d}_{frame}", normalize(cell, 32, 32))

    # Animal color variants (premium). Variant 0 stays the free-pack
    # default sheets (chicken_*/cow_* above); 1-4 come from the premium
    # full-animation sheets: idle = row 0 frames 0/1, walk = the walk row
    # (row 2 for chickens, row 1 for cows) frames 0/1. Babies (what a
    # freshly tamed animal looks like until it grows up) use idle only.
    for v, fname in enumerate(["chicken brown.png", "chicken blue.png",
                               "chicken green.png", "chicken red.png"], start=1):
        sheet = load(PREMIUM / "Animals/Chicken" / fname)
        emit(f"chicken{v}_0", sheet.crop((0, 0, 16, 16)))
        emit(f"chicken{v}_1", sheet.crop((16, 0, 32, 16)))
        emit(f"chicken{v}_w0", sheet.crop((0, 32, 16, 48)))
        emit(f"chicken{v}_w1", sheet.crop((16, 32, 32, 48)))
    for v, fname in enumerate(["Brown cow animations.png",
                               "Green cow animation sprites.png",
                               "Pink cow animation sprites.png",
                               "Purple cow animation sprites.png"], start=1):
        sheet = load(PREMIUM / "Animals/Cow" / fname)
        emit(f"cow{v}_0", sheet.crop((0, 0, 32, 32)))
        emit(f"cow{v}_1", sheet.crop((32, 0, 64, 32)))
        emit(f"cow{v}_w0", sheet.crop((0, 32, 32, 64)))
        emit(f"cow{v}_w1", sheet.crop((32, 32, 64, 64)))
    for v, fname in enumerate(["Chicken_Baby.png", "Chicken_Baby_Brown.png",
                               "Chicken_Baby_Blue.png", "Chicken_Baby_Green.png",
                               "Chicken_Baby_Red.png"]):
        sheet = load(PREMIUM / "Animals/Chicken_Baby" / fname)
        emit(f"chick{v}_0", sheet.crop((0, 0, 16, 16)))
        emit(f"chick{v}_1", sheet.crop((16, 0, 32, 16)))
    for v, fname in enumerate(["baby light cow animations sprites.png",
                               "baby brown cow animations sprites.png",
                               "baby green cow animations sprites.png",
                               "baby pink cow animations sprites.png",
                               "baby purple cow animations sprites.png"]):
        sheet = load(PREMIUM / "Animals/Cow_Baby" / fname)
        emit(f"calf{v}_0", sheet.crop((0, 0, 32, 32)))
        emit(f"calf{v}_1", sheet.crop((32, 0, 64, 32)))

    # --- Windowed premium crops (sprites not on clean 16px cells) -----------
    # Alpha-component analysis of the sheets gave these exact boxes.
    # Buildings from the Chikcen_Houses sheet: small orange house = Coop,
    # medium green house = Barn, small teal house = Camp.
    houses = load(HOUSES)
    emit("place_coop", normalize(houses.crop((193, 81, 223, 123)), 48, 48))
    emit("place_barn", normalize(houses.crop((5, 127, 43, 174)), 48, 48))
    emit("place_camp", normalize(houses.crop((193, 129, 223, 171)), 48, 48))
    # Rock tiers 1/2: boulder cluster and the mossy boulder (physically
    # bigger = harder to mine; world_scene draws each at its own size).
    mush = load(MUSH)
    emit("prop_rock2", normalize(mush.crop((100, 27, 125, 47)), 32, 24))
    # The mossy boulder physically touches its neighbor in the sheet, so
    # the window starts past the contact point (x=162) and keep_largest()
    # sweeps up any remaining stray pixels.
    emit("prop_rock3", normalize(keep_largest(mush.crop((162, 8, 192, 48))), 40, 40))
    # Mushroom tier 2: the skull mushroom (wider than a cell - 32x16 canvas).
    emit("prop_mushroom3", normalize(mush.crop((96, 0, 128, 16)), 32, 16))
    # Chopped-tree stump: the wide-rooted stump from the premium stumps row.
    trees = load(TREES)
    emit("prop_stump", normalize(trees.crop((38, 99, 56, 111)), 24, 16))
    # Mailbox: first (closed, idle) frame of the animation sheet.
    emit("place_mailbox", normalize(load(MAILBOX).crop((0, 16, 32, 56)), 16, 32))
    # Big snow-covered tree (Snowlands tier 1+): full 35px-wide tree
    # (alpha bbox x=103..137) on a 40x48 canvas - ~3 tiles wide on screen.
    emit("tree_snow_big", normalize(load(WINTER_SPRITES).crop((103, 0, 138, 48)), 40, 48))
    # UI slot frames: tan (normal) and raised-white (selected), both
    # normalized to 28x28 so the HUD draws them interchangeably.
    ui = load(UI_BASIC)
    emit("ui_slot", normalize(ui.crop((59, 11, 85, 37)), 28, 28))
    emit("ui_slot_sel", normalize(ui.crop((11, 11, 37, 39)), 28, 28))
    # Mine: the lantern-lit doorway (dungeon walls sheet, bottom-left) is
    # both the overworld Mine Shaft placeable and the in-mine exit portal.
    emit("place_mineshaft", normalize(load(D_WALLS).crop((0, 320, 32, 352)), 32, 32))
    # Bat fly frames (32px pitch; wings mid / wings down), normalized.
    bat = load(D_BAT)
    emit("bat_0", normalize(bat.crop((32, 0, 64, 32)), 32, 32))
    emit("bat_1", normalize(bat.crop((96, 0, 128, 32)), 32, 32))
    # Mine cart (side view), normalized onto a tile-width canvas.
    # Minecarts: the sheet has THREE views (side/E-W with visible wheels,
    # front-back/N-S, and diagonal for corners) - the old 24px crop caught
    # the first cart plus a slice of the second.
    carts_im = load(D_CARTS)
    emit("cart_ew", normalize(keep_largest(carts_im.crop((0, 12, 16, 32))), 16, 24))
    emit("cart_ns", normalize(keep_largest(carts_im.crop((16, 12, 32, 32))), 16, 24))

    # Composite roof pieces for 2D roof areas (see the roof_* comment in
    # SPRITES): scallop-topped and all-shingle variants per column.
    roof_im = load(ROOF)
    for cname, cx in [("l", 48), ("m", 64), ("m2", 80), ("r", 96)]:
        top = Image.new("RGBA", (16, 32), (0, 0, 0, 0))
        top.paste(roof_im.crop((cx, 32, cx + 16, 48)), (0, 0))   # scalloped top row
        top.paste(roof_im.crop((cx, 64, cx + 16, 80)), (0, 16))  # shingles
        emit(f"rooftop_{cname}", top)
        fill = Image.new("RGBA", (16, 32), (0, 0, 0, 0))
        fill.paste(roof_im.crop((cx, 64, cx + 16, 80)), (0, 0))
        fill.paste(roof_im.crop((cx, 64, cx + 16, 80)), (0, 16))
        emit(f"rooffill_{cname}", fill)

    # Sorry-pack wooden furniture: every piece comes in four wood colors
    # matching our tree biomes. Wood group row origins on the sheet:
    # cherry=0, birch=32, oak=64, pine=96; we order variants oak-first so
    # variant 0 (the default) is classic oak.
    SFURN = SORRY / "Early Access/Plant update 2/Furniture/new Wooden Furniture.png"
    sfurn = load(SFURN)
    for w, gy in enumerate([64, 32, 0, 96]):  # oak, birch, cherry, pine
        emit(f"table_w{w}", normalize(sfurn.crop((80, gy, 96, gy + 32)), 16, 32))
        emit(f"dresser_w{w}", normalize(sfurn.crop((0, gy, 16, gy + 32)), 16, 32))
        emit(f"stool_w{w}", normalize(sfurn.crop((96, gy, 112, gy + 32)), 16, 32))
        emit(f"bench_w{w}", normalize(sfurn.crop((112, gy, 144, gy + 32)), 32, 32))
    # Chairs: four facings (right/left/back/front), oak wood - A on a
    # placed chair rotates it.
    for f, col in enumerate([1, 2, 3, 4]):
        emit(f"chair_{f}", normalize(sfurn.crop((col * 16, 64, col * 16 + 16, 96)), 16, 32))
    # Beds: oak frame, three blanket colors (green/blue/pink).
    for v, col in enumerate([9, 10, 11]):
        emit(f"bed_{v}", normalize(sfurn.crop((col * 16, 64, col * 16 + 16, 96)), 16, 32))

    # Wood-typed chests (Sorry pack): closed + fully-open frames, one per
    # biome wood, plus the silver/gold upgrade tiers (same sheet layout).
    # Source chests are ~16px; scaled 2x so they keep the same
    # on-screen size as the old basic-pack chest.
    for name, fname in [("oak", "Oak_Chest.png"), ("birch", "Birch_Chest.png"),
                        ("cherry", "Cherry_Chest.png"), ("pine", "Pine_Chest.png"),
                        ("silver", "silver_chest.png"), ("gold", "golden_chest.png")]:
        csheet = load(SORRY / "Early Access/Plant update 2/Furniture" / fname)
        for state, x0 in [("", 0), ("_open", 128)]:
            frame = csheet.crop((x0, 0, x0 + 32, 32))
            bbox = frame.getbbox()
            if bbox:
                frame = frame.crop(bbox)
            frame = frame.resize((frame.width * 2, frame.height * 2), Image.NEAREST)
            emit(f"chest_{name}{state}", normalize(frame, 32, 32))
    # Christmas tree: two frames of the light-twinkle animation.
    xmas = load(XMAS)
    emit("place_xmas_0", normalize(xmas.crop((0, 0, 48, 64)), 48, 64))
    emit("place_xmas_1", normalize(xmas.crop((48, 0, 96, 64)), 48, 64))
    # Door: composite of the facade top band over the free pack's
    # closed-door frame, matching the rebuilt place_wall's top band so a
    # door drops seamlessly into a wall run.
    door = Image.new("RGBA", (16, 32), (0, 0, 0, 0))
    door.paste(load(WALLS).crop((16, 0, 32, 16)), (0, 0))
    door.alpha_composite(load(DOORS).crop((0, 16, 16, 32)), (0, 16))
    emit("place_door", door)

    # --- Procedural variants -------------------------------------------------
    # Hammer: neither pack draws one (the premium "hammer" cell is another
    # pickaxe), so build a square-head mallet in-style, with every color
    # sampled from the pick sprite itself. The license explicitly allows
    # additional sprites made in the pack's style.
    emit("tool_hammer", make_mallet(generated["tool_pickaxe"]))
    # A real hoe icon after all: the Basic pack's "tools and meterials"
    # sheet's top-right cell (watering can / axe / HOE on its top row).
    emit("tool_hoe", load(BASIC / "Objects/Basic tools and meterials.png").crop((32, 0, 48, 16)))

    # Workbench: the pack's real work station (drawer + tools on top) -
    # the game's crafting station (and the first thing a new player
    # builds, straight from a pile of wood).
    emit("place_workbench", normalize(load(WORKSTATION), 32, 32))

    # Animal-area furnishings (Barn structures sheet - troughs and hay,
    # not a barn building). Hay bale small/long are decoTier variants of
    # one placeable; the full trough composites a hay pile into the box.
    barns = load(BARNS)
    emit("place_haybale", normalize(barns.crop((0, 17, 16, 31)), 16, 16))
    emit("place_haybale_l", normalize(barns.crop((1, 33, 29, 47)), 32, 16))
    trough_full = generated["place_trough"].copy()
    hay = keep_largest(barns.crop((16, 50, 32, 63)))
    trough_full.alpha_composite(hay.crop(hay.getbbox()).resize((12, 8), Image.NEAREST), (2, 3))
    emit("place_trough_full", trough_full)

    # Rowboat (premium Boats sheet, plain hull) - decorative, water-only.
    emit("place_boat", normalize(keep_largest(load(BOATS).crop((6, 43, 39, 58))), 48, 16))

    # Picnic set: blanket + basket + two foods, composited into one 3x2-
    # tile spread (blanket art is 41x31, not cell-aligned).
    blanket = load(PIKNIK_BLANKET).crop((3, 9, 44, 40))
    picnic = Image.new("RGBA", (48, 32), (0, 0, 0, 0))
    picnic.alpha_composite(blanket.resize((44, 32), Image.NEAREST), (2, 0))
    picnic.alpha_composite(load(PIKNIK_BASKET), (4, 6))
    foods = load(PIKNIK_FOODS)
    picnic.alpha_composite(foods.crop((19, 5, 28, 14)), (26, 8))
    picnic.alpha_composite(foods.crop((35, 22, 44, 30)), (32, 18))
    emit("place_picnic", picnic)

    # Presents (winter pack): frame 0 of each color strip; decoTier picks
    # the variant on a placed present.
    for i, pf in enumerate(PRESENTS):
        emit(f"place_present_{i}", load(pf).crop((0, 0, 16, 16)))

    # Deep-mine props (dungeon_probs sheet): crate, sack, tall pot -
    # hash-scattered flavor on mine floors.
    probs = load(D_PROBS)
    emit("prop_crate", normalize(keep_largest(probs.crop((36, 3, 60, 29))), 32, 32))
    emit("prop_sack", normalize(keep_largest(probs.crop((16, 32, 32, 48))), 16, 16))
    emit("prop_pot", normalize(keep_largest(probs.crop((80, 0, 96, 32))), 16, 32))

    # Dedicated fishing wait poses (rod out over the water, 2-frame bob):
    # row 2 of each 48px fishing sheet; side faces right, mirrored for
    # left. The cast swing itself stays the act_rod paperdoll set.
    for sheet_path, d in [(FISH_FRONT, "down"), (FISH_BACK, "up"), (FISH_SIDE, "right")]:
        fsheet = load(sheet_path)
        for frame in range(2):
            cell = fsheet.crop((frame * 48, 96, frame * 48 + 48, 144))
            emit(f"fishwait_{d}_{frame}", normalize(cell, 32, 32))
            if d == "right":
                emit(f"fishwait_left_{frame}",
                     normalize(cell.transpose(Image.FLIP_LEFT_RIGHT), 32, 32))

    # Tree-chop leaf-poof: 13 pre-aligned 64x48 frames (tree shakes, tips,
    # bursts into leaves). Emitted raw - frame-to-frame alignment matters;
    # the standing tree's trunk base sits at (40,44) in frame space.
    tfall = load(TREEFALL)
    for i in range(13):
        emit(f"treefall_{i}", tfall.crop((i * 64, 0, i * 64 + 64, 48)))

    # --- Prep-time paperdoll -------------------------------------------------
    # No pack ships toolless ACTION frames (every swing has its tool baked
    # in), but the ingredients are all here: the walk spritesheet is pure
    # body, so its palette identifies tool paint in the action frames.
    # Erasing non-body colors from the hoe rows yields a toolless swing
    # base; the axe rows' tool-pixel centroids give the natural in-hand
    # anchor per direction/frame. Compositing any 16px tool icon at that
    # anchor mints a full action set - adding new equipment is one entry
    # in EQUIP_TOOLS plus a line in the game's pose table.
    body_colors = set()
    cpx = char.load()
    for yy in range(char.height):
        for xx in range(char.width):
            p = cpx[xx, yy]
            if p[3]:
                body_colors.add(p[:3])

    def tool_pixels(cell):
        px2 = cell.load()
        pts = []
        for yy in range(cell.height):
            for xx in range(cell.width):
                p = px2[xx, yy]
                if p[3] and p[:3] not in body_colors:
                    pts.append((xx, yy))
        return pts

    act_base_cells = {}
    for d, row in ACTION_ROWS.items():
        for frame in range(2):
            cell = actions.crop((frame * 48, row * 48, frame * 48 + 48, row * 48 + 48)).copy()
            px2 = cell.load()
            for (xx, yy) in tool_pixels(cell):
                px2[xx, yy] = (0, 0, 0, 0)
            act_base_cells[(d, frame)] = cell
            emit(f"act_base_{d}_{frame}", normalize(cell, 32, 32))

    anchors = {}
    for d, row in ACTION_ROWS.items():
        for frame in range(2):
            cell = actions.crop((frame * 48, (4 + row) * 48, frame * 48 + 48, (4 + row) * 48 + 48))
            pts = tool_pixels(cell)
            if pts:
                anchors[(d, frame)] = (sum(p[0] for p in pts) // len(pts),
                                       sum(p[1] for p in pts) // len(pts))
            else:
                anchors[(d, frame)] = (24, 16)

    EQUIP_TOOLS = {"hammer": "tool_hammer", "pick": "tool_pickaxe", "rod": "tool_rod"}
    for equip, iconname in EQUIP_TOOLS.items():
        # Inventory icons fill their 16px cell; in-hand the baked tools
        # are ~12px, so downscale to match before compositing.
        icon = generated[iconname].resize((12, 12), Image.NEAREST)
        for d, row in ACTION_ROWS.items():
            for frame in range(2):
                cell = act_base_cells[(d, frame)].copy()
                ic = icon.transpose(Image.FLIP_LEFT_RIGHT) if d == "left" else icon
                ax, ay = anchors[(d, frame)]
                cell.alpha_composite(ic, (max(0, ax - 6), max(0, ay - 6)))
                emit(f"act_{equip}_{d}_{frame}", normalize(cell, 32, 32))
    # Small bat: the deep-mine variant (Sorry pack dungeon enemies).
    sbat = load(DUNGEON / "enemies/small_bat_animations.png")
    emit("smallbat_0", normalize(sbat.crop((32, 0, 64, 32)), 32, 32))
    emit("smallbat_1", normalize(sbat.crop((96, 0, 128, 32)), 32, 32))

    # Village houses (Sorry pack): grand decorative homes. Cottage = blue
    # small house, Hut = green hut, Manor = the brick house.
    vhouse = load(SORRY / "Early Access/Village pack/houses/small house/small_House_with_door_grass.png")
    emit("place_cottage", normalize(vhouse.crop((64, 128, 128, 192)), 64, 64))
    vhut = load(SORRY / "Early Access/Village pack/houses/small hut/small_huts_with_doors_grass.png")
    emit("place_hut", normalize(vhut.crop((0, 0, 64, 64)), 64, 64))
    vbrick = load(SORRY / "Early Access/Village pack/houses/Grey brick house/grey_brick_houses_with_doors_grass.png")
    # The brick houses are ~83px wide (alpha span x=5..88 for the ivy-covered
    # first variant) - a 48px crop only caught the left half of the house.
    emit("place_manor", normalize(vbrick.crop((5, 5, 88, 77)), 96, 80))
    # Snowman with a top hat (winter pack).
    emit("place_snowman", normalize(load(WINTER_SPRITES).crop((128, 64, 144, 96)), 16, 32))
    # Swimming frames: 2 per direction (down/up/left/right rows), pulled
    # from the Ocean pack's 48px-cell swim sheet.
    swim = load(SORRY / "Early Access/Ocean Pack/swimming.png")
    for r, d in enumerate(["down", "up", "left", "right"]):
        for f in range(2):
            cell = swim.crop((f * 48 + 8, r * 48 + 8, f * 48 + 40, r * 48 + 40))
            emit(f"swim_{d}_{f}", normalize(cell, 24, 24))
    # Emote bubbles (UI pack Teemo emotes): heart (tamed!) and cheer (level
    # up!) - drawn over the player's head for a moment.
    emotes = load(ITCH / "Sprout Lands - UI Pack - Basic pack/Sprite sheets/Dialouge UI/Emotes/Teemo Basic emote animations sprite sheet.png")
    emit("emote_heart", normalize(emotes.crop((0, 128, 32, 160)), 32, 32))
    emit("emote_cheer", normalize(emotes.crop((0, 192, 32, 224)), 32, 32))
    # Real speech bubble behind the level-up cheer (UI pack emoji
    # bubbles): the full-size grey bubble with its bottom tail, plus a
    # widened variant (interior columns stretched either side of the
    # tail, which stays centered) that fits the cheer AND the skill icon.
    # Emitted at natural size - the draw code places them by exact dims.
    bub = load(ITCH / "Sprout Lands - UI Pack - Basic pack/emojis-free/speech_bubble_grey.png").crop((11, 11, 53, 58))  # 42x47
    emit("emote_bubble", bub)
    wide = Image.new("RGBA", (76, 47), (0, 0, 0, 0))
    wide.paste(bub.crop((0, 0, 12, 47)), (0, 0))
    wide.paste(bub.crop((12, 0, 13, 47)).resize((17, 47), Image.NEAREST), (12, 0))
    wide.paste(bub.crop((12, 0, 30, 47)), (29, 0))
    wide.paste(bub.crop((29, 0, 30, 47)).resize((17, 47), Image.NEAREST), (47, 0))
    wide.paste(bub.crop((30, 0, 42, 47)), (64, 0))
    emit("emote_bubble_wide", wide)
    # Small request bubble over a wild animal's head (what it wants to be
    # tamed with). The premium emoji sheet's pop-in animation supplies a
    # mid-pop droplet and the settled small bubble - two frames of "pop"
    # juice before the item icon appears. Emitted at natural size like the
    # big bubbles above.
    pops = load(ITCH / "Sprout Lands - UI Pack - Premium pack/emojis/speech_bubble_grey.png")
    emit("req_bubble_0", pops.crop((2 * 64 + 23, 64 + 38, 2 * 64 + 41, 64 + 60)))  # 18x22
    emit("req_bubble_1", pops.crop((3 * 64 + 17, 64 + 24, 3 * 64 + 48, 64 + 56)))  # 31x32
    # Bottom-screen tab frames: hollow 9-slice rings built from the UI
    # pack's tan dialog box, pre-composed at the two exact sizes the HUD
    # needs (both drawn at 2x - Inventory's ring spans the whole area
    # below the header, Skills' starts below the held-item line).
    # Crop at x11 to drop the box's left-pointing speech tail (y21..31);
    # edge strips are single-pixel slices from tail-free zones stretched
    # between 6px corner slices (corner rounding is ~3px).
    dlg = load(ITCH / "Sprout Lands - UI Pack - Basic pack/Sprite sheets/Dialouge UI/dialog box.png").crop((11, 11, 37, 39))  # 26x28 box
    def frame_ring(w, h):
        c = 6            # corner slice
        et, eb, es = 4, 5, 4  # top/bottom/side edge thickness
        bw, bh = dlg.size
        ring = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        ring.paste(dlg.crop((c, 0, c + 1, et)).resize((w - 2 * c, et), Image.NEAREST), (c, 0))
        ring.paste(dlg.crop((c, bh - eb, c + 1, bh)).resize((w - 2 * c, eb), Image.NEAREST), (c, h - eb))
        ring.paste(dlg.crop((0, c, es, c + 1)).resize((es, h - 2 * c), Image.NEAREST), (0, c))
        ring.paste(dlg.crop((bw - es, c, bw, c + 1)).resize((es, h - 2 * c), Image.NEAREST), (w - es, c))
        ring.paste(dlg.crop((0, 0, c, c)), (0, 0))
        ring.paste(dlg.crop((bw - c, 0, bw, c)), (w - c, 0))
        ring.paste(dlg.crop((0, bh - c, c, bh)), (0, h - c))
        ring.paste(dlg.crop((bw - c, bh - c, bw, bh)), (w - c, h - c))
        return ring
    emit("ui_frame_inv", frame_ring(160, 104))
    emit("ui_frame_skills", frame_ring(160, 97))
    # Egg-hatch: a freshly tamed chicken starts as a wobbling egg.
    chick_eggs = load(PREMIUM / "Animals/Chicken_Baby/Chicken_Baby.png")
    emit("egg_0", normalize(chick_eggs.crop((48, 240, 64, 256)), 16, 16))
    emit("egg_1", normalize(chick_eggs.crop((64, 240, 80, 256)), 16, 16))
    # Gate/door open states (drawn when the player is close - they swing
    # open for you). The open leaves come from the swing animation's
    # mid-open frame (alpha span x=85..105: left leaf 85..96, right leaf
    # 96..105) and are pasted hinge-side-flush - left leaf hugs its tile's
    # left edge, right leaf the right - instead of bottom-centered.
    gates_im = load(GATES)
    gl = keep_largest(gates_im.crop((85, 0, 94, 16)))
    gl = gl.crop(gl.getbbox()) if gl.getbbox() else gl
    leaf = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    leaf.paste(gl, (0, 16 - gl.height))
    emit("place_gate_l_open", leaf)
    gr = keep_largest(gates_im.crop((94, 0, 105, 16)))
    gr = gr.crop(gr.getbbox()) if gr.getbbox() else gr
    leaf = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    leaf.paste(gr, (16 - gr.width, 16 - gr.height))
    emit("place_gate_r_open", leaf)
    door_open = Image.new("RGBA", (16, 32), (0, 0, 0, 0))
    door_open.paste(load(WALLS).crop((16, 0, 32, 16)), (0, 0))
    door_open.alpha_composite(load(DOORS).crop((0, 0, 16, 16)), (0, 16))
    emit("place_door_open", door_open)
    # Bush tiers: exotic recolors (whole-sprite hue rotation) - the premium
    # bush row isn't cleanly separable, and the recolor-tier look was the
    # original design idea anyway.
    emit("prop_bush2", hue_shift(generated["prop_bush"], 45))
    emit("prop_bush3", hue_shift(generated["prop_bush"], 150))
    # Full watering can: blue-tinted. Gold ore: gold-tinted stone. Hole: mud
    # rim with a sunken pit inset (see make_hole).
    emit("tool_watering_can_full", tint(generated["tool_watering_can"], (140, 180, 245)))
    emit("item_ore", tint(generated["item_stone"], (240, 200, 105)))
    emit("tile_hole", make_hole(generated["tile_dirt"]))

    # Watered-crop variants: every growth-stage sprite gets a wet-mound
    # twin (crop_<species>_<stage>w), drawn when the bed is watered.
    CROP_SPECIES = ["wheat", "turnip", "carrot", "tomato", "pumpkin",
                    "cauliflower", "eggplant", "lettuce", "radish",
                    "beetroot", "starfruit", "cucumber", "corn", "sunflower"]
    for sp in CROP_SPECIES:
        for stage in range(4):
            emit(f"crop_{sp}_{stage}w", wetten(generated[f"crop_{sp}_{stage}"]))

    manifest = GFX_DIR / "atlas.t3s"
    # NOTE: the anti-bleed "-b edge" border option lives in the Makefile's
    # tex3ds invocation - the in-file options parser doesn't accept it.
    lines = ["--atlas -f rgba8888 -z auto"] + [f"{name}.png" for name in generated]
    manifest.write_text("\n".join(lines) + "\n")
    print(f"wrote gfx/atlas.t3s ({len(generated)} sprites)")

    convert_audio()


# Game-event name -> source WAV in the Sorry pack's Audio folder. The WAVs
# are 22050Hz 16-bit stereo padded with long silence; they get downmixed to
# mono, silence-trimmed, and written as raw PCM16 to sfx/*.pcm, which the
# Makefile copies into romfs:/sfx/ (see platform/audio.cpp for playback).
SOUNDS = {
    "till": "squick_1.wav",
    "dig": "squick_2.wav",
    "chop": "punch_1.wav",
    "mine": "punch_3.wav",
    "demolish": "punch_5.wav",
    "plant": "blup_1.wav",
    "place": "blup_2.wav",
    "harvest": "bing_1.wav",
    "levelup": "flute_1.wav",
    "deny": "boo_1.wav",
    "ui": "bip_1.wav",
    # The rest of the pack's WAVs, mapped to the events they fit:
    "bite": "phone_2.wav",            # fish on the line - attention ring
    "splash": "squick_squick_1.wav",  # reel-in catch / entering water
    "eat": "squick_squick_2.wav",     # drinking a potion
    "hurt": "boo_2.wav",              # player takes damage
    "hit": "punch_2.wav",             # striking an enemy
    "kill": "punch_6.wav",            # enemy defeated
    "bonk": "punch_4.wav",            # hammer-bonking a rock
    "mail": "phone_1.wav",            # checking the mailbox
    "chime": "flute_2.wav",           # furniture variant cycle
    "tame": "flute_3.wav",            # taming a wild animal
    "descend": "flute_4.wav",         # riding the mineshaft down
}


def convert_audio():
    SFX_DIR.mkdir(exist_ok=True)
    for old in SFX_DIR.glob("*.pcm"):
        old.unlink()
    for name, src in SOUNDS.items():
        with wave.open(str(AUDIO / src)) as w:
            assert w.getsampwidth() == 2 and w.getframerate() == 22050
            raw = w.readframes(w.getnframes())
            ch = w.getnchannels()
        samples = struct.unpack(f"<{len(raw)//2}h", raw)
        if ch == 2:
            samples = [(samples[i] + samples[i+1]) // 2 for i in range(0, len(samples), 2)]
        # Trim: keep from just before the first audible sample to shortly
        # after the last, capped at 2s so nothing hogs romfs/linear memory.
        thresh = 350
        first = next((i for i, s in enumerate(samples) if abs(s) > thresh), 0)
        last = next((i for i in range(len(samples)-1, -1, -1) if abs(samples[i]) > thresh), 0)
        start = max(0, first - 220)          # ~10ms lead-in
        end = min(len(samples), last + 3310, start + 44100)  # +150ms tail, cap 2s
        trimmed = samples[start:end]
        (SFX_DIR / f"{name}.pcm").write_bytes(struct.pack(f"<{len(trimmed)}h", *trimmed))
        print(f"wrote sfx/{name}.pcm ({len(trimmed)/22050.0:.2f}s from {src})")


if __name__ == "__main__":
    main()
