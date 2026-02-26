#pragma once

#include "fabric/core/ChunkStreaming.hh"
#include "fabric/core/Event.hh"
#include "fabric/core/VoxelMesher.hh"

#include <unordered_map>
#include <unordered_set>

namespace fabric {

inline constexpr const char* kVoxelChangedEvent = "voxel_changed";

struct ChunkMeshConfig {
    int maxRemeshPerTick = 4;
    float threshold = 0.5f;
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
};

} // namespace fabric
