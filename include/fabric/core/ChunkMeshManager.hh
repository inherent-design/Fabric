#pragma once

#include "fabric/core/ChunkStreaming.hh"
#include "fabric/core/Event.hh"
#include "fabric/core/VertexPool.hh"
#include "fabric/core/VoxelMesher.hh"

#include <unordered_map>
#include <unordered_set>

namespace fabric {

inline constexpr const char* kVoxelChangedEvent = "voxel_changed";

struct ChunkMeshConfig {
    int maxRemeshPerTick = 4;
    float threshold = 0.5f;
    float lodDistance1 = 64.0f;  // Distance threshold for LOD 0 -> 1
    float lodDistance2 = 128.0f; // Distance threshold for LOD 1 -> 2
    float lodHysteresis = 4.0f;  // Dead zone width to prevent LOD thrashing
};

class ChunkMeshManager {
  public:
    ChunkMeshManager(EventDispatcher& dispatcher, const ChunkedGrid<float>& density,
                     const ChunkedGrid<Vector4<float, Space::World>>& essence, ChunkMeshConfig config = {});
    ~ChunkMeshManager();

    // Mark a chunk as needing re-mesh (by chunk coordinates)
    void markDirty(int cx, int cy, int cz);

    // Process dirty chunks up to per-tick budget. Returns number of chunks re-meshed.
    int update();

    const ChunkMeshData* meshFor(const ChunkCoord& coord) const;
    bool isDirty(const ChunkCoord& coord) const;
    size_t dirtyCount() const;
    size_t meshCount() const;

    void removeChunk(const ChunkCoord& coord);

    // LOD management
    void setChunkLOD(const ChunkCoord& coord, int level);
    int getChunkLOD(const ChunkCoord& coord) const;
    void updateLOD(float cameraX, float cameraY, float cameraZ);
    static int computeLODLevel(float distance, float lodDist1, float lodDist2, float hysteresis, int currentLOD);

    // Pool-based GPU buffer management
    void initPool();
    void initPool(const VertexPool::Config& config);
    void shutdownPool();
    VertexPool* pool();
    const VertexPool* pool() const;
    const PoolSlot* slotFor(const ChunkCoord& coord) const;

    // Emit a voxel_changed event (convenience for callers who modify grids)
    static void emitVoxelChanged(EventDispatcher& dispatcher, int cx, int cy, int cz);

  private:
    EventDispatcher& dispatcher_;
    const ChunkedGrid<float>& density_;
    const ChunkedGrid<Vector4<float, Space::World>>& essence_;
    ChunkMeshConfig config_;
    std::string handlerId_;

    std::unordered_set<ChunkCoord, ChunkCoordHash> dirty_;
    std::unordered_map<ChunkCoord, ChunkMeshData, ChunkCoordHash> meshes_;
    std::unordered_map<ChunkCoord, int, ChunkCoordHash> chunkLODs_;

    void appendSkirtGeometry(ChunkMeshData& data, const ChunkCoord& coord, int lod);

    VertexPool pool_;
    std::unordered_map<ChunkCoord, PoolSlot, ChunkCoordHash> slots_;
};

} // namespace fabric
