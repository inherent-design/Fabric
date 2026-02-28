#include "fabric/core/ChunkMeshManager.hh"

#include <algorithm>
#include <cmath>

namespace fabric {

ChunkMeshManager::ChunkMeshManager(EventDispatcher& dispatcher, const ChunkedGrid<float>& density,
                                   const ChunkedGrid<Vector4<float, Space::World>>& essence,
                                   const ChunkMeshConfig& config)
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

        int lod = getChunkLOD(coord);
        auto data =
            VoxelMesher::meshChunkData(coord.cx, coord.cy, coord.cz, density_, essence_, config_.threshold, lod);

        appendSkirtGeometry(data, coord, lod);

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
    chunkLODs_.erase(coord);

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

void ChunkMeshManager::setChunkLOD(const ChunkCoord& coord, int level) {
    auto [it, inserted] = chunkLODs_.emplace(coord, level);
    if (!inserted && it->second != level) {
        it->second = level;
        dirty_.insert(coord);
    }
    if (inserted && level != 0) {
        dirty_.insert(coord);
    }
}

int ChunkMeshManager::getChunkLOD(const ChunkCoord& coord) const {
    auto it = chunkLODs_.find(coord);
    return (it != chunkLODs_.end()) ? it->second : 0;
}

int ChunkMeshManager::computeLODLevel(float distance, float lodDist1, float lodDist2, float hysteresis,
                                      int currentLOD) {
    if (currentLOD == 0) {
        if (distance > lodDist2 + hysteresis)
            return 2;
        if (distance > lodDist1 + hysteresis)
            return 1;
        return 0;
    }

    if (currentLOD == 1) {
        if (distance > lodDist2 + hysteresis)
            return 2;
        if (distance < lodDist1 - hysteresis)
            return 0;
        return 1;
    }

    // currentLOD == 2
    if (distance < lodDist1 - hysteresis)
        return 0;
    if (distance < lodDist2 - hysteresis)
        return 1;
    return 2;
}

void ChunkMeshManager::updateLOD(float cameraX, float cameraY, float cameraZ) {
    constexpr float kHalfChunk = static_cast<float>(kChunkSize) / 2.0f;

    for (const auto& [coord, mesh] : meshes_) {
        float cx = static_cast<float>(coord.cx * kChunkSize) + kHalfChunk;
        float cy = static_cast<float>(coord.cy * kChunkSize) + kHalfChunk;
        float cz = static_cast<float>(coord.cz * kChunkSize) + kHalfChunk;

        float dx = cameraX - cx;
        float dy = cameraY - cy;
        float dz = cameraZ - cz;
        float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

        int currentLOD = getChunkLOD(coord);
        int newLOD =
            computeLODLevel(distance, config_.lodDistance1, config_.lodDistance2, config_.lodHysteresis, currentLOD);

        if (newLOD != currentLOD) {
            setChunkLOD(coord, newLOD);
        }
    }
}

void ChunkMeshManager::appendSkirtGeometry(ChunkMeshData& data, const ChunkCoord& coord, int lod) {
    const int stride = 1 << lod;
    const int lodSize = kChunkSize / stride;

    // Only handle X and Z boundary faces (vertical skirts dropping in -Y)
    struct SkirtFace {
        int face;
        int normalAxis;
        int uAxis;
        bool positive;
    };
    constexpr SkirtFace kSkirtFaces[] = {
        {0, 0, 2, true},  // +X boundary
        {1, 0, 2, false}, // -X boundary
        {4, 2, 0, true},  // +Z boundary
        {5, 2, 0, false}, // -Z boundary
    };

    for (const auto& sf : kSkirtFaces) {
        ChunkCoord neighbor = coord;
        if (sf.normalAxis == 0) {
            neighbor.cx += sf.positive ? 1 : -1;
        } else {
            neighbor.cz += sf.positive ? 1 : -1;
        }

        auto nit = chunkLODs_.find(neighbor);
        if (nit == chunkLODs_.end() || nit->second == lod)
            continue;

        int neighborStride = 1 << nit->second;
        int skirtDrop = std::max(stride, neighborStride);
        int boundaryLocal = sf.positive ? kChunkSize : 0;

        for (int vy = 0; vy < lodSize; ++vy) {
            for (int vu = 0; vu < lodSize; ++vu) {
                int cell[3];
                cell[sf.normalAxis] = sf.positive ? (kChunkSize - stride) : 0;
                cell[1] = vy * stride;
                cell[sf.uAxis] = vu * stride;

                int wx = coord.cx * kChunkSize + cell[0];
                int wy = coord.cy * kChunkSize + cell[1];
                int wz = coord.cz * kChunkSize + cell[2];

                if (density_.get(wx, wy, wz) <= config_.threshold)
                    continue;

                int yTop = cell[1];
                int yBot = std::max(0, yTop - skirtDrop);
                if (yBot >= yTop)
                    continue;

                int u0 = cell[sf.uAxis];
                int u1 = u0 + stride;

                auto bi = static_cast<uint32_t>(data.vertices.size());
                auto fn = static_cast<uint8_t>(sf.face);
                uint16_t pi = 0;

                auto bv = static_cast<uint8_t>(boundaryLocal);
                auto yt = static_cast<uint8_t>(yTop);
                auto yb = static_cast<uint8_t>(yBot);
                auto cu0 = static_cast<uint8_t>(u0);
                auto cu1 = static_cast<uint8_t>(u1);

                // Vertex winding matches VoxelMesher per-face convention
                if (sf.normalAxis == 0) {
                    if (sf.positive) {
                        // +X (face 0): kVertUV ordering
                        data.vertices.push_back(VoxelVertex::pack(bv, yb, cu1, fn, 0, pi));
                        data.vertices.push_back(VoxelVertex::pack(bv, yb, cu0, fn, 0, pi));
                        data.vertices.push_back(VoxelVertex::pack(bv, yt, cu0, fn, 0, pi));
                        data.vertices.push_back(VoxelVertex::pack(bv, yt, cu1, fn, 0, pi));
                    } else {
                        // -X (face 1)
                        data.vertices.push_back(VoxelVertex::pack(bv, yb, cu0, fn, 0, pi));
                        data.vertices.push_back(VoxelVertex::pack(bv, yb, cu1, fn, 0, pi));
                        data.vertices.push_back(VoxelVertex::pack(bv, yt, cu1, fn, 0, pi));
                        data.vertices.push_back(VoxelVertex::pack(bv, yt, cu0, fn, 0, pi));
                    }
                } else {
                    if (sf.positive) {
                        // +Z (face 4)
                        data.vertices.push_back(VoxelVertex::pack(cu0, yb, bv, fn, 0, pi));
                        data.vertices.push_back(VoxelVertex::pack(cu1, yb, bv, fn, 0, pi));
                        data.vertices.push_back(VoxelVertex::pack(cu1, yt, bv, fn, 0, pi));
                        data.vertices.push_back(VoxelVertex::pack(cu0, yt, bv, fn, 0, pi));
                    } else {
                        // -Z (face 5)
                        data.vertices.push_back(VoxelVertex::pack(cu1, yb, bv, fn, 0, pi));
                        data.vertices.push_back(VoxelVertex::pack(cu0, yb, bv, fn, 0, pi));
                        data.vertices.push_back(VoxelVertex::pack(cu0, yt, bv, fn, 0, pi));
                        data.vertices.push_back(VoxelVertex::pack(cu1, yt, bv, fn, 0, pi));
                    }
                }

                data.indices.push_back(bi + 0);
                data.indices.push_back(bi + 1);
                data.indices.push_back(bi + 2);
                data.indices.push_back(bi + 0);
                data.indices.push_back(bi + 2);
                data.indices.push_back(bi + 3);
            }
        }
    }
}

} // namespace fabric
