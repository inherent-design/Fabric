#include "fabric/core/ChunkMeshManager.hh"

namespace fabric {

ChunkMeshManager::ChunkMeshManager(EventDispatcher& dispatcher, const ChunkedGrid<float>& density,
                                   const ChunkedGrid<Vector4<float, Space::World>>& essence, ChunkMeshConfig config)
    : dispatcher_(dispatcher), density_(density), essence_(essence), config_(config) {
    handlerId_ = dispatcher_.addEventListener(kVoxelChangedEvent, [this](Event& e) {
        int cx = e.getData<int>("cx");
        int cy = e.getData<int>("cy");
        int cz = e.getData<int>("cz");
        dirty_.insert({cx, cy, cz});
    });
}

ChunkMeshManager::~ChunkMeshManager() {
    shutdownPool();
    dispatcher_.removeEventListener(kVoxelChangedEvent, handlerId_);
}

void ChunkMeshManager::markDirty(int cx, int cy, int cz) {
    dirty_.insert({cx, cy, cz});
}

int ChunkMeshManager::update() {
    int count = 0;
    auto it = dirty_.begin();
    while (it != dirty_.end() && count < config_.maxRemeshPerTick) {
        auto coord = *it;
        it = dirty_.erase(it);

        auto data = VoxelMesher::meshChunkData(coord.cx, coord.cy, coord.cz, density_, essence_, config_.threshold);

        // Upload to pool if initialized
        if (pool_.isValid()) {
            // Free old slot if re-meshing an existing chunk
            auto slotIt = slots_.find(coord);
            if (slotIt != slots_.end()) {
                pool_.free(slotIt->second);
                slots_.erase(slotIt);
            }

            if (!data.vertices.empty()) {
                PoolSlot slot = pool_.allocate(data.vertices.data(), static_cast<uint32_t>(data.vertices.size()),
                                               data.indices.data(), static_cast<uint32_t>(data.indices.size()));
                if (slot.valid())
                    slots_[coord] = slot;
            }
        }

        meshes_[coord] = std::move(data);
        ++count;
    }
    return count;
}

const ChunkMeshData* ChunkMeshManager::meshFor(const ChunkCoord& coord) const {
    auto it = meshes_.find(coord);
    if (it == meshes_.end())
        return nullptr;
    return &it->second;
}

bool ChunkMeshManager::isDirty(const ChunkCoord& coord) const {
    return dirty_.contains(coord);
}

size_t ChunkMeshManager::dirtyCount() const {
    return dirty_.size();
}

size_t ChunkMeshManager::meshCount() const {
    return meshes_.size();
}

void ChunkMeshManager::removeChunk(const ChunkCoord& coord) {
    dirty_.erase(coord);
    meshes_.erase(coord);

    auto slotIt = slots_.find(coord);
    if (slotIt != slots_.end()) {
        pool_.free(slotIt->second);
        slots_.erase(slotIt);
    }
}

void ChunkMeshManager::initPool() {
    pool_.init();
}

void ChunkMeshManager::initPool(const VertexPool::Config& config) {
    pool_.init(config);
}

void ChunkMeshManager::shutdownPool() {
    slots_.clear();
    pool_.shutdown();
}

VertexPool* ChunkMeshManager::pool() {
    return pool_.isValid() ? &pool_ : nullptr;
}

const VertexPool* ChunkMeshManager::pool() const {
    return pool_.isValid() ? &pool_ : nullptr;
}

const PoolSlot* ChunkMeshManager::slotFor(const ChunkCoord& coord) const {
    auto it = slots_.find(coord);
    if (it == slots_.end())
        return nullptr;
    return &it->second;
}

void ChunkMeshManager::emitVoxelChanged(EventDispatcher& dispatcher, int cx, int cy, int cz) {
    Event e(kVoxelChangedEvent, "ChunkMeshManager");
    e.setData<int>("cx", cx);
    e.setData<int>("cy", cy);
    e.setData<int>("cz", cz);
    dispatcher.dispatchEvent(e);
}

} // namespace fabric
