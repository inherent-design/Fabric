#include "fabric/simulation/SimulationGrid.hh"

namespace fabric::simulation {

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

// -- SimulationGrid -----------------------------------------------------------

SimulationGrid::SimulationGrid() = default;

int64_t SimulationGrid::packKey(int cx, int cy, int cz) {
    return (static_cast<int64_t>(cx) << 42) | (static_cast<int64_t>(cy & 0x1FFFFF) << 21) |
           static_cast<int64_t>(cz & 0x1FFFFF);
}

int SimulationGrid::readIndex() const {
    return static_cast<int>(epoch_ & 1);
}

int SimulationGrid::writeIndex() const {
    return static_cast<int>((epoch_ + 1) & 1);
}

VoxelCell SimulationGrid::readFromBuffer(int wx, int wy, int wz, int bufferIdx) const {
    int cx, cy, cz, lx, ly, lz;
    recurse::ChunkedGrid<VoxelCell>::worldToChunk(wx, wy, wz, cx, cy, cz, lx, ly, lz);
    auto key = packKey(cx, cy, cz);
    auto it = chunks_.find(key);
    if (it == chunks_.end())
        return VoxelCell{};
    const auto& pair = it->second;
    if (!pair.isMaterialized())
        return pair.fillValue;
    int idx = lx + ly * kChunkSize + lz * kChunkSize * kChunkSize;
    return (*pair.buffers[bufferIdx])[idx];
}

VoxelCell SimulationGrid::readCell(int wx, int wy, int wz) const {
    return readFromBuffer(wx, wy, wz, readIndex());
}

VoxelCell SimulationGrid::readFromWriteBuffer(int wx, int wy, int wz) const {
    return readFromBuffer(wx, wy, wz, writeIndex());
}

void SimulationGrid::writeCell(int wx, int wy, int wz, VoxelCell cell) {
    int cx, cy, cz, lx, ly, lz;
    recurse::ChunkedGrid<VoxelCell>::worldToChunk(wx, wy, wz, cx, cy, cz, lx, ly, lz);
    auto key = packKey(cx, cy, cz);
    auto& pair = chunks_[key];
    if (!pair.isMaterialized())
        pair.materialize();
    int idx = lx + ly * kChunkSize + lz * kChunkSize * kChunkSize;
    (*pair.buffers[writeIndex()])[idx] = cell;
}

void SimulationGrid::advanceEpoch() {
    // Propagate latest state: copy write buffer → read buffer so the next
    // epoch's write buffer (which is the current read buffer after swap)
    // starts with the complete current state.
    for (auto& [key, pair] : chunks_) {
        if (pair.isMaterialized())
            *pair.buffers[readIndex()] = *pair.buffers[writeIndex()];
    }
    ++epoch_;
}

uint64_t SimulationGrid::currentEpoch() const {
    return epoch_;
}

void SimulationGrid::fillChunk(int cx, int cy, int cz, VoxelCell fill) {
    auto key = packKey(cx, cy, cz);
    auto& pair = chunks_[key];
    pair.fillValue = fill;
    // Keep as sentinel -- do not allocate buffers
}

void SimulationGrid::materializeChunk(int cx, int cy, int cz) {
    auto key = packKey(cx, cy, cz);
    auto& pair = chunks_[key];
    pair.materialize();
}

bool SimulationGrid::isChunkMaterialized(int cx, int cy, int cz) const {
    auto key = packKey(cx, cy, cz);
    auto it = chunks_.find(key);
    if (it == chunks_.end())
        return false;
    return it->second.isMaterialized();
}

size_t SimulationGrid::materializedChunkCount() const {
    size_t count = 0;
    for (const auto& [_, pair] : chunks_) {
        if (pair.isMaterialized())
            ++count;
    }
    return count;
}

const std::array<VoxelCell, kChunkVolume>* SimulationGrid::readBuffer(int cx, int cy, int cz) const {
    auto key = packKey(cx, cy, cz);
    auto it = chunks_.find(key);
    if (it == chunks_.end())
        return nullptr;
    const auto& pair = it->second;
    if (!pair.isMaterialized())
        return nullptr;
    return pair.buffers[readIndex()].get();
}

std::array<VoxelCell, kChunkVolume>* SimulationGrid::writeBuffer(int cx, int cy, int cz) {
    auto key = packKey(cx, cy, cz);
    auto it = chunks_.find(key);
    if (it == chunks_.end())
        return nullptr;
    auto& pair = it->second;
    if (!pair.isMaterialized())
        pair.materialize();
    return pair.buffers[writeIndex()].get();
}

bool SimulationGrid::hasChunk(int cx, int cy, int cz) const {
    return chunks_.find(packKey(cx, cy, cz)) != chunks_.end();
}

VoxelCell SimulationGrid::getChunkFillValue(int cx, int cy, int cz) const {
    auto it = chunks_.find(packKey(cx, cy, cz));
    if (it == chunks_.end())
        return VoxelCell{}; // Return default (Air)
    return it->second.fillValue;
}

void SimulationGrid::removeChunk(int cx, int cy, int cz) {
    chunks_.erase(packKey(cx, cy, cz));
}

void SimulationGrid::clear() {
    chunks_.clear();
    epoch_ = 0;
}

std::vector<std::tuple<int, int, int>> SimulationGrid::allChunks() const {
    std::vector<std::tuple<int, int, int>> result;
    result.reserve(chunks_.size());
    for (const auto& [key, _] : chunks_) {
        // Unpack key back to chunk coordinates
        int cx = static_cast<int>(key >> 42);
        int cy = static_cast<int>((key >> 21) & 0x1FFFFF);
        int cz = static_cast<int>(key & 0x1FFFFF);
        // Sign-extend 21-bit values
        if (cy & 0x100000)
            cy |= ~0x1FFFFF;
        if (cz & 0x100000)
            cz |= ~0x1FFFFF;
        result.emplace_back(cx, cy, cz);
    }
    return result;
}

} // namespace fabric::simulation
