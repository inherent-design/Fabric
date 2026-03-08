#include "recurse/simulation/SimulationGrid.hh"
#include "fabric/world/ChunkCoordUtils.hh"

namespace recurse::simulation {

using fabric::ChunkedGrid;

// -- SimulationGrid -----------------------------------------------------------

SimulationGrid::SimulationGrid() = default;

int SimulationGrid::readIndex() const {
    return static_cast<int>(epoch_ & 1);
}

int SimulationGrid::writeIndex() const {
    return static_cast<int>((epoch_ + 1) & 1);
}

VoxelCell SimulationGrid::readFromBuffer(int wx, int wy, int wz, int bufferIdx) const {
    int cx, cy, cz, lx, ly, lz;
    ChunkedGrid<VoxelCell>::worldToChunk(wx, wy, wz, cx, cy, cz, lx, ly, lz);
    const auto* pair = registry_.find(cx, cy, cz);
    if (!pair)
        return VoxelCell{};
    if (!pair->isMaterialized())
        return pair->fillValue;
    int idx = lx + ly * K_CHUNK_SIZE + lz * K_CHUNK_SIZE * K_CHUNK_SIZE;
    return (*pair->buffers[bufferIdx])[idx];
}

VoxelCell SimulationGrid::readCell(int wx, int wy, int wz) const {
    return readFromBuffer(wx, wy, wz, readIndex());
}

VoxelCell SimulationGrid::readFromWriteBuffer(int wx, int wy, int wz) const {
    return readFromBuffer(wx, wy, wz, writeIndex());
}

void SimulationGrid::writeCell(int wx, int wy, int wz, VoxelCell cell) {
    int cx, cy, cz, lx, ly, lz;
    ChunkedGrid<VoxelCell>::worldToChunk(wx, wy, wz, cx, cy, cz, lx, ly, lz);
    auto& pair = registry_.addChunk(cx, cy, cz);
    if (!pair.isMaterialized())
        pair.materialize();
    int idx = lx + ly * K_CHUNK_SIZE + lz * K_CHUNK_SIZE * K_CHUNK_SIZE;
    (*pair.buffers[writeIndex()])[idx] = cell;
}

bool SimulationGrid::writeCellIfExists(int wx, int wy, int wz, VoxelCell cell) {
    int cx, cy, cz, lx, ly, lz;
    ChunkedGrid<VoxelCell>::worldToChunk(wx, wy, wz, cx, cy, cz, lx, ly, lz);
    auto* pair = registry_.find(cx, cy, cz);
    if (!pair)
        return false;
    if (!pair->isMaterialized())
        return false;
    int idx = lx + ly * K_CHUNK_SIZE + lz * K_CHUNK_SIZE * K_CHUNK_SIZE;
    (*pair->buffers[writeIndex()])[idx] = cell;
    return true;
}

void SimulationGrid::writeCellImmediate(int wx, int wy, int wz, VoxelCell cell) {
    int cx, cy, cz, lx, ly, lz;
    ChunkedGrid<VoxelCell>::worldToChunk(wx, wy, wz, cx, cy, cz, lx, ly, lz);
    auto& pair = registry_.addChunk(cx, cy, cz);
    if (!pair.isMaterialized())
        pair.materialize();
    int idx = lx + ly * K_CHUNK_SIZE + lz * K_CHUNK_SIZE * K_CHUNK_SIZE;
    (*pair.buffers[readIndex()])[idx] = cell;
    (*pair.buffers[writeIndex()])[idx] = cell;
}

void SimulationGrid::advanceEpoch() {
    int ri = readIndex();
    int wi = writeIndex();
    registry_.forEachMaterialized([ri, wi](ChunkBufferPair& pair) { *pair.buffers[ri] = *pair.buffers[wi]; });
    ++epoch_;
}

uint64_t SimulationGrid::currentEpoch() const {
    return epoch_;
}

void SimulationGrid::fillChunk(int cx, int cy, int cz, VoxelCell fill) {
    auto& pair = registry_.addChunk(cx, cy, cz);
    pair.fillValue = fill;
}

void SimulationGrid::materializeChunk(int cx, int cy, int cz) {
    auto& pair = registry_.addChunk(cx, cy, cz);
    pair.materialize();
}

bool SimulationGrid::isChunkMaterialized(int cx, int cy, int cz) const {
    const auto* pair = registry_.find(cx, cy, cz);
    if (!pair)
        return false;
    return pair->isMaterialized();
}

size_t SimulationGrid::materializedChunkCount() const {
    return registry_.materializedChunkCount();
}

const std::array<VoxelCell, K_CHUNK_VOLUME>* SimulationGrid::readBuffer(int cx, int cy, int cz) const {
    const auto* pair = registry_.find(cx, cy, cz);
    if (!pair)
        return nullptr;
    if (!pair->isMaterialized())
        return nullptr;
    return pair->buffers[readIndex()].get();
}

std::array<VoxelCell, K_CHUNK_VOLUME>* SimulationGrid::writeBuffer(int cx, int cy, int cz) {
    auto* pair = registry_.find(cx, cy, cz);
    if (!pair)
        return nullptr;
    if (!pair->isMaterialized())
        pair->materialize();
    return pair->buffers[writeIndex()].get();
}

bool SimulationGrid::hasChunk(int cx, int cy, int cz) const {
    return registry_.hasChunk(cx, cy, cz);
}

VoxelCell SimulationGrid::getChunkFillValue(int cx, int cy, int cz) const {
    const auto* pair = registry_.find(cx, cy, cz);
    if (!pair)
        return VoxelCell{};
    return pair->fillValue;
}

void SimulationGrid::removeChunk(int cx, int cy, int cz) {
    registry_.removeChunk(cx, cy, cz);
}

void SimulationGrid::clear() {
    registry_.clear();
    epoch_ = 0;
}

size_t SimulationGrid::chunkCount() const {
    return registry_.chunkCount();
}

std::vector<std::tuple<int, int, int>> SimulationGrid::allChunks() const {
    return registry_.allChunks();
}

ChunkRegistry& SimulationGrid::registry() {
    return registry_;
}

const ChunkRegistry& SimulationGrid::registry() const {
    return registry_;
}

} // namespace recurse::simulation
