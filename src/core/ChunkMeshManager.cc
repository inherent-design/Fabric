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
        meshes_[coord] =
            VoxelMesher::meshChunkData(coord.cx, coord.cy, coord.cz, density_, essence_, config_.threshold);
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
}

void ChunkMeshManager::emitVoxelChanged(EventDispatcher& dispatcher, int cx, int cy, int cz) {
    Event e(kVoxelChangedEvent, "ChunkMeshManager");
    e.setData<int>("cx", cx);
    e.setData<int>("cy", cy);
    e.setData<int>("cz", cz);
    dispatcher.dispatchEvent(e);
}

} // namespace fabric
