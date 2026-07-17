# MyFarm

A homebrew 3DS survival/farming game. No NPCs, no shops - you spawn into an
open, procedurally generated world with **just five tools**, and everything
else is earned from the world itself: forage bushes for seeds and berries,
chop trees for wood and saplings, hammer rocks for stone and ore, dig holes
and pour water to sculpt ponds, build fences/paths/bridges/chests and a
camp (your teleport home), tame wild chickens, cows, pigs, and alpacas
(multiple colors each, tamed as babies that grow up - every color has
its own two favorite foods), win over a cat with fresh fish (it moves in
at your Camp), spot foxes, wolves, wild slimes, and strolling shroomlings
out in the far country, go fishing, keep bees, and level ten
RuneScape-style skills (Farming, Logging, Mining, Foraging, Mycology,
Herding, Fishing, Building, Athletics, Swimming - capped at level 100,
each level slower than the last) to unlock
higher-tier resource nodes found farther from spawn (fruit trees,
boulders, skull mushrooms, exotic bushes). The world is split into
biomes - meadow, birch, cherry-blossom, and pine regions, plus the
permanent-winter **Snowlands** in the far north where ponds freeze into
walkable ice - and every action has a sound effect. A full day/night
cycle runs every 48 real minutes (lamps and campfires glow at night),
15-minute weather fronts bring rain (snow in the Snowlands) that makes
fish bite faster, and you can build a house tile-by-tile from Walls,
Floors, and Doors. Crops, tree regrowth,
node respawns, eggs, and milk all run on **real wall-clock time** - close
the app, come back later, the world moved on. The top screen renders in
real stereoscopic 3D via the console's depth slider.

Milestones 1 (open world + farm loop), 2 (gathering, taming, terrain
editing, building, skills), 3 (the multi-floor mine with combat), and the
Grand Finale (swimming, watered crops, 13 crop species, 13 kinds of sea
life + sunken treasure, village homes, emotes, hatching eggs, gates and
doors that swing open) are all complete; see [`CREDITS.md`](CREDITS.md)
for art attribution.

## 1. Get a toolchain

### Option A: container (recommended, nothing installed locally)

[Containerfile](Containerfile) builds on the official `devkitpro/devkitarm`
image (which already ships devkitARM, libctru, and citro2d/citro3d), adding
`makerom`/`bannertool` (built from source - devkitPro doesn't package
prebuilt ones compatible with this image). All you need on the host is
Podman.

```powershell
.\build.ps1          # make cia   -> myfarm.cia
.\build.ps1 3dsx     # make       -> myfarm.3dsx
.\build.ps1 clean
```

The first run builds the image (a few minutes); later runs reuse it.

Equivalent raw commands, if you'd rather not use the script:

```
podman build -t myfarm-builder -f Containerfile .
podman run --rm -v ${PWD}:/project myfarm-builder make cia
```

### Option B: native devkitPro install

Install [devkitPro](https://devkitpro.org/wiki/Getting_Started) (its
installer sets `DEVKITPRO`/`DEVKITARM` for you), then:

```
(dkp-)pacman -S 3ds-dev general-tools 3ds-citro2d 3ds-citro3d
```

- `3ds-dev` - devkitARM + libctru
- `general-tools` - `makerom` + `bannertool`, needed to produce the `.cia`
- `3ds-citro2d` / `3ds-citro3d` - the 2D rendering this game uses

Then `make` (→ `myfarm.3dsx`) or `make cia` (→ `myfarm.cia`).

## 2. Install and run

Prebuilt binaries live on the
[Releases page](https://github.com/whatadeer/MyFarm/releases) - every
release includes a QR code: scan it with FBI (Remote Install → Scan QR
Code) and the console downloads and installs `myfarm.cia` straight from
GitHub, no SD card shuffling. Pushes to `main` also upload fresh builds
as Actions artifacts.

- **Sideload on real hardware (CFW/Luma3DS)**: copy `myfarm.cia` to your SD
  card and install it with [FBI](https://github.com/Steveice10/FBI).
- **Homebrew Launcher / Citra**: copy `myfarm.3dsx` to `/3ds/` on your SD
  card, or open it directly in [Citra](https://citra-emu.org/).

## 3. Controls

| Input | Action |
|---|---|
| Circle Pad / D-Pad | Move (paths and rails are faster). Push the Circle Pad all the way to its rim to **sprint** - or swim hard in open water - burning stamina (the D-Pad always walks) |
| A | Contextual action on the faced tile: pick up a dropped item / harvest a ripe crop / forage a bush, mushroom, or ripe fruit tree (all toolless) / feed a wild animal to tame it / collect from a Coop, Barn, or Beehive / open a Chest (an open chest
mirrors its contents on the top screen). With a tool selected: Axe chops trees, Pickaxe mines rocks, Watering Can scoops from water (or a Well) and, when full, waters a crop (1.5x growth) or pours into a dug hole to make a pond, Hammer bonks trees/rocks for a small freebie without depleting them (or places the loaded ghost building elsewhere - see the ghost preview), seeds plant, a Sapling plants into a hole |
| B | Dig a hole / fill a hole / un-till / demolish the faced placed object (no tool needed). Also closes menus/chests |
| L / R | Cycle the bottom-screen tab (Inventory / Skills) |
| X / Y | With the Hammer equipped: cycle which building the ghost preview is loaded with |
| Touch: Inventory tab | Tap a slot to select it; tap the selected slot again (or any empty slot) to unequip to empty hands. DROP/TRASH buttons act on whatever's equipped - Drop places it on the ground in front of you (pick it back up later), Trash destroys it |
| SELECT | Screenshot to `sdmc:/3ds/myfarm/screenshots/` (BMP). With the 3D slider up it saves a side-by-side **stereoscopic pair** |
| START | Pause menu: Save, Teleport Home, Resume |

The top screen shows **contextual button prompts** (bottom-left): what A
and B would do right now - Harvest, Chop, Open, Sleep, Ride, Dig, and so
on - so you never have to guess. Level-ups pop the cheering emote plus
the **skill's tool icon** (hoe/axe/pickaxe/berries/egg/rod) over your
head.

**Building**: there's no separate build menu - just equip the Hammer and a
transparent ghost of the currently-loaded building appears on the tile
you're facing, green when it can be placed there and affordable, red when
it can't (blocked, water, or not enough Wood/Stone). X/Y cycles through
every buildable; A places it directly, spending raw Wood/Stone straight
from your inventory - no separate craft-then-place step.

New saves start with ONE tool: an Axe. Chop trees, then with the Wood
selected press A to build a **Workbench** (6 wood, no hammer needed - the
ghost preview shows where it lands). A on the bench opens the crafting
menu: Hammer, Hoe, Pickaxe, Watering Can, and Fishing Rod (stone comes
from digging holes with B). Tilling takes the Hoe; digging stays
toolless. Skills don't appear in the Skills tab at all until you first
perform them - the game reveals itself as you play.

**Water finds its level**: dig a hole beside water and it floods in -
and rushes through any connected trench of holes. Pouring the watering
can into a hole floods its neighbors too. Canal-building, the honest
way. New games also spawn on the nearest open tile, never inside a rock
or tree.

**The loop**: forage a bush → get seeds (higher-tier bushes drop Carrot/
Tomato/Pumpkin seeds) → till, plant, wait real minutes, harvest → chop
wood → build a Coop + Camp → ask a wild chicken what it fancies (every
color has TWO favorite foods and coyly shows just one per ask - feed it
either) → it moves in as a baby, grows up, lays eggs on a timer → craft a
Fishing Rod and a Beehive → level up → unlock the boulders, fruit trees,
and skull mushrooms you've spotted out past the 50-tile mark, and wander
into birch, cherry-blossom, and pine country beyond that.

**Sound**: every action has a Sprout Lands sound effect. On real hardware
this needs the DSP firmware dump most CFW setups already have
(`sdmc:/3ds/dspfirm.cdc`, created by running the DSP1 homebrew once) - if
it's missing the game simply plays silently.

**Running & stamina**: push the Circle Pad to its rim and you sprint
(1.55x on land; in open water it's a hard swim). Sprinting burns the
green stamina bar (top-left of the top screen, hidden while full) and
trains **Athletics** - every level grows the pool. Hard swimming trains
**Swimming** instead - every level makes it cheaper. Empty the bar and
you're winded (the bar turns red): no sprinting until it refills a
quarter of the way. The D-Pad always walks, and ice keeps its own
momentum rules. (Save v12 - older saves load fine.)

**Home building**: equip the Hammer, cycle to Floor (1 wood), Wall
(3 wood), Door (4 wood), or Roof (4 wood) with X/Y, and lay them out
tile-by-tile - floor first, walls around it, a door in the gap, a roof
run along the top. Walls in horizontal runs get the premium facade's
framed trim at each end (middles stay plank panels), roofs get shingle
edge trims, and fences use the premium pack's full 16-piece set - corners,
T-junctions, and 4-way crossings connect automatically, including into
gate leaves. Walls block,
doors and floors don't, furniture (chest/lamp/chair/rug/campfire) places
on floor, and the shovel (B) rips any of it back up. Houses are open-roof
by design - the top-down camera looks straight into your rooms.

**Day, night & weather**: a full day lasts 48 real minutes (driven by the
same wall clock as the crops, so time passes while the app is closed).
Nights genuinely darken the top screen; Lamps, Campfires, and Xmas Trees
cast warm light. Weather changes on 15-minute fronts - rain speeds up
fishing bites, and in the Snowlands it falls as snow.

**The Snowlands**: walk far enough north (past roughly y = -180 - the
tree line is jagged) and the world turns to permanent winter: snow
ground, snow-covered pines, and frozen ponds you can walk across.

**The Grand Finale**: step into any water and you **swim** (slower, and
no working with wet hands); the **Watering Can finally waters** - a
watered crop (darker soil) grows 1.5x faster, once per planting;
**Corn** joins as the 13th crop and grows two tiles tall; the fishing
pools now hold **13 kinds of sea life** (shrimp to sea turtle) plus rare
**sunken-treasure gems** at Fishing 4+; the Hammer's build list tops out
with the village-pack **Cottage, Hut, and Manor** and a top-hat
**Snowman**;
freshly tamed chickens arrive as a **wobbling egg that hatches**; gates
and doors **swing open** as you approach; and the little farmer pops a
**heart emote** on tames and a **cheer** on level-ups.

Gates come as **separate left and right leaves** - place a Gate (Left)
alone for a small gate, or pair it with a Gate (Right) on the next tile
for a wide double gate. Control hints in the HUD use **real button
glyphs** (Vryell's Controllers & Keyboard pack).

**Minecarts**: stand on a rail with empty-ish hands and press A to hop
in - the cart follows the track at speed, taking corners on its own,
until the line ends or you hop out (A or B). Works in the Mine too:
press A standing on a floor's rail run and ride it - enemies don't stop
chasing, so it's a joyride through danger. The mine's parked carts got
their real art too (the sheet has side, front, and diagonal views).

**Sleeping**: press A on a Bed at night (20:00-6:00) to sleep to 6am -
the whole world fast-forwards with you (crops grow, nodes respawn,
animals produce, weather moves on) and your hearts refill. During the
day, A restyles the bed instead. (Save format v7 - v6 saves still load.)

**Roof areas**: roofs aren't just strips anymore - build them out in 2D
and each tile picks its art from its position: scalloped ridge along the
exposed top row, the trim band one row down, shingle fill below, with
left/right edge trims throughout.

**Furniture & restyling**: Beds (8 wood), Tables (5 wood), Dressers
(6 wood), Stools (2 wood), 2-tile Benches (4 wood), and long 2-tile Rugs
(3 wood) join Chairs, Lamps, and small Rugs - and pressing A on any
placed piece of furniture cycles it: chairs rotate through four facings,
tables/dressers/stools/benches swap between oak/birch/cherry/pine wood,
rugs, beds, and lamps change color. Chests take the wood of the biome you place them in
(oak in the meadow, birch/cherry/pine in their forests). Sorry-pack
furniture art throughout.

**The path family**: four kinds of walk-faster ground, all in the
Hammer's build cycle - **Stone Path** (1 stone, grey with scattered
stones), **Dirt Path** (1 wood, plain trodden soil), **Plank Path**
(1 wood, laid boards), and **Rail** (1 wood 1 stone) - cart tracks as a
special type of path, drawn with the dungeon pack's track pieces:
straights, quarter-turn corners, and crossings connect automatically as
you lay them. Different path types butt together seamlessly, and the
shovel (B) rips any of them back up.

**The Building skill** (7th skill): placing anything with the Hammer
earns Building XP (bigger builds teach more), and the X/Y build cycle
only shows what your level unlocks - fences and a camp at level 1, house
parts at 3, roofs and bridges at 4, coops and rails at 5, the barn and
mine shaft at 6, and the village homes at 7-9. Level-ups announce "new
blueprints!". (Save v8; older saves load with Building at level 1.)

**Paperdoll tool poses**: the pickaxe, hammer, and fishing rod now show
their real selves in your hands during actions - prep-time composites of
the tool icon onto a toolless swing base (extracted from the packs'
frames via the walk sheet's body palette, anchored where the baked axe
sits per frame). Empty-handed actions swing empty hands. Adding new
equipment art is one line in `prep_assets.py`'s EQUIP_TOOLS plus one
case in the pose table.

**Autotiled ground**: paths, tilled beds, wooden floors, the snow line,
shorelines, and mine floors all use the packs' full blob tilesets - a
4-bit neighbor mask picks corners, edges, T-junctions, caps, and islands
at draw time, so paths genuinely turn, tilled beds get rounded borders,
grass overhangs water, the Snowlands boundary has a real melt line, and
mine rooms look carved out of the rock. Wooden floors have no blob in any
pack, so theirs is generated (plank texture in the soil blob's silhouette
with a shadowed rim). Mine-cart tracks are connection-aware too: rail
runs now bend, using the dungeon pack's quarter-turn corner pieces.
Purely visual - saves are untouched.

**The Mine** (Milestone 3): craft a **Mine Shaft** (10 wood + 5 stone),
place it, press A to descend. Floors are roguelike rooms - regenerated
fresh every visit, lit only by your torchlight. **A** mines the ore node
or ladder you face, or swings your held tool at enemies (Axe/Pickaxe hit
for 2); **B** always swings. Slimes hop at you, bats swoop over the
rocks; contact costs half a heart (three hearts total, shown top-left).
Eat food (berries/apples/fish/honey/potions) with A to heal. Ore gets
richer with depth: stone and coal near the top, then copper, gold, and
**gems** - rubies, diamonds, emeralds, amethyst - past floor 5. Slimes
sometimes drop healing potions. Blacking out just sends you back to the
shaft - you keep everything. The exit portal (lantern doorway) climbs
back up one floor at a time.

## 4. Tests

`tests/` has host-native unit tests (plain g++, no devkitARM/Podman needed)
for every module with logic worth checking without a real 3DS: crop
growth timing, tile legality, deterministic world generation, the chunk
cache, inventory stacking, and save/load round-tripping. Rendering, input
polling, and the scene wiring are thin wrappers around citro2d/libctru or
so entangled with 3DS hardware that a test would mostly exercise a mock,
so those are left to manual testing on real hardware/Citra.

Requires a host g++ (if you don't have one, `devkitpro/devkitarm`'s image
happens to ship a native Debian g++ alongside its ARM cross-compiler, so
`podman run --rm -v ${PWD}:/project -w /project/tests devkitpro/devkitarm make`
works too, no separate install needed):

```
make -C tests            # build + run all tests
make -C tests coverage   # build + run with gcov line coverage
make -C tests clean
```

## 5. Regenerating art from the Sprout Lands pack

The actual sprite pixels aren't hand-drawn by this repo - `tools/prep_assets.py`
slices specific tiles out of the Sprout Lands zip (path hardcoded to
`~/Downloads/Sprout Lands - Sprites - Basic pack.zip`) into `gfx/`, which
the Makefile's romfs/tex3ds pipeline then bin-packs into
`romfs:/gfx/atlas.t3x` at build time. Re-run it if you add/change which
tiles are used:

```
python tools/prep_assets.py
```

`meta/gen_placeholder_assets.py` regenerates `meta/icon.png`,
`meta/banner.png` (both composited from Sprout Lands tiles), and
`meta/audio.wav` (silent - bannertool needs *some* audio file even though
the game has no sound yet).

Both scripts need Pillow (`pip install pillow`).

## Notes / things worth knowing

- **World**: chunk-based (16x16 tiles/chunk), procedurally generated from
  a per-save random seed - deterministic, so an unmodified chunk is never
  saved, only regenerated identically on demand as you walk into it. Only
  chunks you've actually tilled/planted get written to the save file.
- **Real-time everything**: crop stages, chopped-tree regrowth, node
  respawns, sapling maturation, and egg/milk production all run through
  one shared formula (`source/core/growth_timer.h`) computed from
  `now - startedAt` every frame *and* once on load, so time passes
  correctly whether the app stayed open or was fully closed. Wheat takes 6
  minutes, Turnip 12; every other duration/cost/XP value lives in
  `source/core/balance.h`.
- **Skills**: RuneScape-style curve (~5%/level, capped at 100 -
  `source/core/skills.cpp`; roughly 99k lifetime XP to max one). Unlocks
  span the whole 1-100 ladder: every tree VARIATION has its own Logging
  requirement (meadow 1 &rarr; birch 8 &rarr; cherry 16 &rarr; pine 24
  &rarr; snow 32, big trees 12-52, fruit trees 65, and the rare 3x3
  Ancient Tree of the far field at 85), rocks gate at
  1/25/50, bushes 1/20/45, mushrooms (Mycology) 1/15/40, each crop
  species has a Farming level to plant, mine ores layer Mining levels
  over floor depth, cows need Herding 12, and the Building list climbs
  all the way to the level-100 Clone Mirror - see `balance.h` and
  `kBuildables`.
- **Tier recolors are procedural**: the autumn tree, copper/gold rocks,
  exotic bushes, the Barn, the Camp, gold Ore, the full Bucket, and the
  hole tile are all hue-shifted/tinted variants generated by
  `tools/prep_assets.py` from base Sprout Lands sprites (the pack's
  license allows modification).
- **Save file**: `sdmc:/3ds/myfarm/save.dat`, a small versioned binary
  format (`source/core/save.cpp`). Autosaves on quit (HOME menu) in
  addition to the pause menu's explicit Save.
- `resources/app.rsf` is the makerom packaging spec (title/permissions),
  pared down from the sibling `homeassist-ds` project's - no networking,
  camera, IR, or mic access, just graphics/input/filesystem. If `makerom`
  ever complains about a field, check here first.
- The "farmer" is Sprout Lands' chibi cat-eared critter, not a human - see
  `tools/prep_assets.py`'s comment on `player_down/up/left/right` for a
  caveat about the exact walk-direction mapping (best guess from the sheet
  layout, not doc-confirmed - swap indices there if a direction looks
  wrong on screen).
