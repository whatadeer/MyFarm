#include "core/world.h"

namespace core {

uint64_t ChunkStore::key(ChunkCoord coord) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(coord.cx)) << 32) |
           static_cast<uint32_t>(coord.cy);
}

Chunk& ChunkStore::getOrGenerate(ChunkCoord coord) {
    uint64_t k = key(coord);
    auto it = chunks_.find(k);
    if (it != chunks_.end()) return it->second;

    return chunks_.emplace(k, generateChunk(worldSeed_, coord)).first->second;
}

Tile& ChunkStore::tileAt(int32_t worldX, int32_t worldY) {
    ChunkCoord coord = worldToChunk(worldX, worldY);
    int lx, ly;
    worldToLocal(worldX, worldY, &lx, &ly);
    return getOrGenerate(coord).at(lx, ly);
}

void ChunkStore::markDirty(int32_t worldX, int32_t worldY) {
    ChunkCoord coord = worldToChunk(worldX, worldY);
    getOrGenerate(coord).dirty = true;
}

std::vector<const Chunk*> ChunkStore::dirtyChunks() const {
    std::vector<const Chunk*> result;
    for (const auto& entry : chunks_) {
        if (entry.second.dirty) result.push_back(&entry.second);
    }
    return result;
}

void ChunkStore::setChunk(const Chunk& chunk) {
    uint64_t k = key(chunk.coord);
    chunks_[k] = chunk;
    chunks_[k].dirty = true;
}

} // namespace core
