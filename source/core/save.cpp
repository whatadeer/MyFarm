#include "core/save.h"

#include <cstring>

namespace core {

namespace {

constexpr char kMagic[4] = {'M', 'Y', 'F', 'M'};
// v11: the tool bar - tools move out of the general inventory grid into
// their own fixed ToolBelt slots (see inventory.h). v10: the Clone Mirror.
// v9: building-interior registry. v8: the Building skill (7th skill slot).
// v7 added the bed-sleep clock offset. Backward compatible down to v6:
// older saves load with the missing fields zeroed/empty instead of being
// treated as "not found" - worlds are big enough now that wiping them
// over an added field would be rude.
constexpr uint8_t kVersion = 11;
constexpr uint8_t kMinVersion = 6;

class ByteWriter {
public:
    void u8(uint8_t v) { bytes_.push_back(v); }

    void u32(uint32_t v) {
        for (int i = 0; i < 4; i++) bytes_.push_back(static_cast<uint8_t>(v >> (i * 8)));
    }

    void i32(int32_t v) { u32(static_cast<uint32_t>(v)); }

    void i64(int64_t v) {
        u32(static_cast<uint32_t>(static_cast<uint64_t>(v) & 0xFFFFFFFFu));
        u32(static_cast<uint32_t>(static_cast<uint64_t>(v) >> 32));
    }

    void f32(float v) {
        uint32_t bits;
        std::memcpy(&bits, &v, sizeof(bits));
        u32(bits);
    }

    void raw(const char* data, size_t n) {
        for (size_t i = 0; i < n; i++) bytes_.push_back(static_cast<uint8_t>(data[i]));
    }

    std::vector<uint8_t> take() { return std::move(bytes_); }

private:
    std::vector<uint8_t> bytes_;
};

class ByteReader {
public:
    explicit ByteReader(const std::vector<uint8_t>& bytes) : bytes_(bytes) {}

    bool ok() const { return ok_; }

    uint8_t u8() {
        if (pos_ + 1 > bytes_.size()) {
            ok_ = false;
            return 0;
        }
        return bytes_[pos_++];
    }

    uint32_t u32() {
        if (pos_ + 4 > bytes_.size()) {
            ok_ = false;
            return 0;
        }
        uint32_t v = 0;
        for (int i = 0; i < 4; i++) v |= static_cast<uint32_t>(bytes_[pos_++]) << (i * 8);
        return v;
    }

    int32_t i32() { return static_cast<int32_t>(u32()); }

    int64_t i64() {
        uint32_t lo = u32();
        uint32_t hi = u32();
        return static_cast<int64_t>((static_cast<uint64_t>(hi) << 32) | lo);
    }

    float f32() {
        uint32_t bits = u32();
        float v;
        std::memcpy(&v, &bits, sizeof(v));
        return v;
    }

private:
    const std::vector<uint8_t>& bytes_;
    size_t pos_ = 0;
    bool ok_ = true;
};

void writeTile(ByteWriter& w, const Tile& t) {
    w.u8(static_cast<uint8_t>(t.terrain));
    w.u8(t.tilled ? 1 : 0);
    w.u8(t.hasCrop ? 1 : 0);
    w.u8(t.watered ? 1 : 0);
    w.u8(t.cropSpeciesId);
    w.i64(t.timestamp);
    w.u8(static_cast<uint8_t>(t.decoration));
    w.u8(t.decoTier);
    w.u8(t.depleted ? 1 : 0);
    w.u8(static_cast<uint8_t>(t.placed));
}

void readTile(ByteReader& r, Tile* t) {
    t->terrain = static_cast<Terrain>(r.u8());
    t->tilled = r.u8() != 0;
    t->hasCrop = r.u8() != 0;
    t->watered = r.u8() != 0;
    t->cropSpeciesId = r.u8();
    t->timestamp = r.i64();
    t->decoration = static_cast<Decoration>(r.u8());
    t->decoTier = r.u8();
    t->depleted = r.u8() != 0;
    t->placed = static_cast<Placed>(r.u8());
}

void writeInventory(ByteWriter& w, const Inventory& inv) {
    for (int i = 0; i < Inventory::slotCount(); i++) {
        const ItemStack& s = inv.slot(i);
        w.u32(s.item);
        w.u32(s.count);
    }
}

void readInventory(ByteReader& r, Inventory* inv) {
    for (int i = 0; i < Inventory::slotCount(); i++) {
        ItemId item = static_cast<ItemId>(r.u32());
        uint16_t count = static_cast<uint16_t>(r.u32());
        inv->setSlot(i, item, count);
    }
}

void writeToolBelt(ByteWriter& w, const ToolBelt& belt) {
    for (int i = 0; i < ToolBelt::slotCount(); i++) {
        const ItemStack& s = belt.slot(i);
        w.u32(s.item);
        w.u32(s.count);
    }
}

void readToolBelt(ByteReader& r, ToolBelt* belt) {
    for (int i = 0; i < ToolBelt::slotCount(); i++) {
        ItemId item = static_cast<ItemId>(r.u32());
        uint16_t count = static_cast<uint16_t>(r.u32());
        belt->setSlot(i, item, count);
    }
}

// Pre-v11 saves kept tools sitting in the general inventory grid. Pull any
// out into their new dedicated slots so old saves don't just lose them.
void migrateToolsFromGrid(Inventory* inv, ToolBelt* belt) {
    for (int i = 0; i < Inventory::slotCount(); i++) {
        const ItemStack& s = inv->slot(i);
        if (s.item != kItemNone && isToolItem(s.item)) {
            belt->add(s.item);
            inv->setSlot(i, kItemNone, 0);
        }
    }
}

} // namespace

std::vector<uint8_t> serializeSave(const GameState& state) {
    ByteWriter w;
    w.raw(kMagic, 4);
    w.u8(kVersion);

    w.u32(state.worldSeed);
    w.f32(state.playerPos.x);
    w.f32(state.playerPos.y);
    w.u8(static_cast<uint8_t>(state.facing));

    w.u8(state.home.set ? 1 : 0);
    w.i32(state.home.x);
    w.i32(state.home.y);
    w.u8(state.hasLastFieldPos ? 1 : 0);
    w.f32(state.lastFieldPos.x);
    w.f32(state.lastFieldPos.y);
    w.i64(state.clockOffset); // v7+

    for (int i = 0; i < kSkillCount; i++) w.u32(state.skillXp[i]);

    writeInventory(w, state.inventory);
    writeToolBelt(w, state.toolBelt); // v11+

    w.u32(static_cast<uint32_t>(state.animals.size()));
    for (const TamedAnimal& a : state.animals) {
        w.u8(static_cast<uint8_t>(a.species));
        w.u8(a.variant);
        w.i32(a.homeX);
        w.i32(a.homeY);
        w.i64(a.tamedAt);
        w.i64(a.lastCollectedAt);
    }

    w.u32(static_cast<uint32_t>(state.chests.size()));
    for (const ChestData& c : state.chests) {
        w.i32(c.x);
        w.i32(c.y);
        writeInventory(w, c.items);
    }

    w.u32(static_cast<uint32_t>(state.hives.size()));
    for (const HiveData& hv : state.hives) {
        w.i32(hv.x);
        w.i32(hv.y);
        w.i64(hv.lastCollectedAt);
    }

    w.u32(static_cast<uint32_t>(state.groundItems.size()));
    for (const GroundItem& g : state.groundItems) {
        w.i32(g.x);
        w.i32(g.y);
        w.u32(g.item);
        w.u32(g.count);
    }

    w.u32(static_cast<uint32_t>(state.interiors.size())); // v9+
    for (const InteriorData& in : state.interiors) {
        w.i32(in.bx);
        w.i32(in.by);
        w.u8(in.kind);
        w.u8(in.wl);
        w.u8(in.wr);
        w.u8(in.h);
    }

    w.u8(state.clone.exists ? 1 : 0); // v10+
    w.f32(state.clone.pos.x);
    w.f32(state.clone.pos.y);
    w.u8(state.clone.task);

    std::vector<const Chunk*> dirty = state.world.dirtyChunks();
    w.u32(static_cast<uint32_t>(dirty.size()));
    for (const Chunk* chunk : dirty) {
        w.i32(chunk->coord.cx);
        w.i32(chunk->coord.cy);
        for (const Tile& t : chunk->tiles) writeTile(w, t);
    }

    return w.take();
}

bool deserializeSave(const std::vector<uint8_t>& bytes, GameState* outState) {
    ByteReader r(bytes);

    char magic[4];
    for (char& c : magic) c = static_cast<char>(r.u8());
    uint8_t version = r.u8();
    if (!r.ok()) return false;
    if (std::memcmp(magic, kMagic, 4) != 0) return false;
    if (version < kMinVersion || version > kVersion) return false;

    uint32_t seed = r.u32();
    GameState state(seed);

    state.playerPos.x = r.f32();
    state.playerPos.y = r.f32();
    state.facing = static_cast<Facing>(r.u8());

    state.home.set = r.u8() != 0;
    state.home.x = r.i32();
    state.home.y = r.i32();
    state.hasLastFieldPos = r.u8() != 0;
    state.lastFieldPos.x = r.f32();
    state.lastFieldPos.y = r.f32();
    state.clockOffset = version >= 7 ? r.i64() : 0;

    int skillsInFile = version >= 8 ? kSkillCount : 6;
    for (int i = 0; i < skillsInFile; i++) state.skillXp[i] = r.u32();

    readInventory(r, &state.inventory);
    if (version >= 11) {
        readToolBelt(r, &state.toolBelt);
    } else {
        migrateToolsFromGrid(&state.inventory, &state.toolBelt);
    }

    uint32_t animalCount = r.u32();
    if (!r.ok()) return false;
    for (uint32_t i = 0; i < animalCount; i++) {
        TamedAnimal a;
        a.species = static_cast<AnimalSpecies>(r.u8());
        a.variant = r.u8();
        a.homeX = r.i32();
        a.homeY = r.i32();
        a.tamedAt = r.i64();
        a.lastCollectedAt = r.i64();
        if (!r.ok()) return false;
        state.animals.push_back(a);
    }

    uint32_t chestCount = r.u32();
    if (!r.ok()) return false;
    for (uint32_t i = 0; i < chestCount; i++) {
        ChestData c;
        c.x = r.i32();
        c.y = r.i32();
        readInventory(r, &c.items);
        if (!r.ok()) return false;
        state.chests.push_back(c);
    }

    uint32_t hiveCount = r.u32();
    if (!r.ok()) return false;
    for (uint32_t i = 0; i < hiveCount; i++) {
        HiveData hv;
        hv.x = r.i32();
        hv.y = r.i32();
        hv.lastCollectedAt = r.i64();
        if (!r.ok()) return false;
        state.hives.push_back(hv);
    }

    uint32_t groundCount = r.u32();
    if (!r.ok()) return false;
    for (uint32_t i = 0; i < groundCount; i++) {
        GroundItem g;
        g.x = r.i32();
        g.y = r.i32();
        g.item = static_cast<ItemId>(r.u32());
        g.count = static_cast<uint16_t>(r.u32());
        if (!r.ok()) return false;
        state.groundItems.push_back(g);
    }

    if (version >= 9) {
        uint32_t interiorCount = r.u32();
        if (!r.ok()) return false;
        for (uint32_t i = 0; i < interiorCount; i++) {
            InteriorData in;
            in.bx = r.i32();
            in.by = r.i32();
            in.kind = r.u8();
            in.wl = r.u8();
            in.wr = r.u8();
            in.h = r.u8();
            if (!r.ok()) return false;
            state.interiors.push_back(in);
        }
    }

    if (version >= 10) {
        state.clone.exists = r.u8() != 0;
        state.clone.pos.x = r.f32();
        state.clone.pos.y = r.f32();
        state.clone.task = r.u8();
        if (!r.ok()) return false;
    }

    uint32_t chunkCount = r.u32();
    if (!r.ok()) return false;

    for (uint32_t c = 0; c < chunkCount; c++) {
        Chunk chunk;
        chunk.coord.cx = r.i32();
        chunk.coord.cy = r.i32();
        for (Tile& t : chunk.tiles) readTile(r, &t);
        if (!r.ok()) return false;
        state.world.setChunk(chunk);
    }

    *outState = std::move(state);
    return true;
}

} // namespace core
