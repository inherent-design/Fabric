#include "recurse/simulation/ChunkRegistry.hh"

namespace recurse::simulation {

// -- ChunkBufferPair ----------------------------------------------------------

void ChunkBufferPair::materialize() {
    if (isMaterialized())
        return;
    buffers[0] = std::make_unique<Buffer>();
    buffers[1] = std::make_unique<Buffer>();
    buffers[0]->fill(fillValue);
    buffers[1]->fill(fillValue);
}

bool ChunkBufferPair::isMaterialized() const {
    return buffers[0] != nullptr;
}

// -- ChunkRegistry ------------------------------------------------------------

ChunkBufferPair& ChunkRegistry::addChunk(int cx, int cy, int cz) {
    auto key = packChunkKey(cx, cy, cz);
    return slots_[key];
}

void ChunkRegistry::removeChunk(int cx, int cy, int cz) {
    slots_.erase(packChunkKey(cx, cy, cz));
}

void ChunkRegistry::clear() {
    slots_.clear();
}

ChunkBufferPair* ChunkRegistry::find(int cx, int cy, int cz) {
    auto it = slots_.find(packChunkKey(cx, cy, cz));
    if (it == slots_.end())
        return nullptr;
    return &it->second;
}

const ChunkBufferPair* ChunkRegistry::find(int cx, int cy, int cz) const {
    auto it = slots_.find(packChunkKey(cx, cy, cz));
    if (it == slots_.end())
        return nullptr;
    return &it->second;
}

bool ChunkRegistry::hasChunk(int cx, int cy, int cz) const {
    return slots_.find(packChunkKey(cx, cy, cz)) != slots_.end();
}

size_t ChunkRegistry::chunkCount() const {
    return slots_.size();
}

size_t ChunkRegistry::materializedChunkCount() const {
    size_t count = 0;
    for (const auto& [_, pair] : slots_) {
        if (pair.isMaterialized())
            ++count;
    }
    return count;
}

std::vector<std::tuple<int, int, int>> ChunkRegistry::allChunks() const {
    std::vector<std::tuple<int, int, int>> result;
    result.reserve(slots_.size());
    for (const auto& [key, _] : slots_) {
        auto [cx, cy, cz] = unpackChunkKey(key);
        result.emplace_back(cx, cy, cz);
    }
    return result;
}

} // namespace recurse::simulation
