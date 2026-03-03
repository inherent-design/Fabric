#pragma once

#include "fabric/core/FieldLayer.hh"
#include "fabric/core/SystemBase.hh"
#include "recurse/render/WaterRenderer.hh"
#include "recurse/world/ChunkStreaming.hh"

#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace recurse {
class WaterSimulation;
} // namespace recurse

namespace recurse::systems {

class TerrainSystem;
class ChunkPipelineSystem;
class ShadowRenderSystem;
class VoxelRenderSystem;

/// Owns the water simulation, water field seeding, water mesh cache, and
/// water renderer. Runs during the Render phase after VoxelRenderSystem so
/// that transparent water geometry composites over opaque terrain.
class WaterRenderSystem : public fabric::System<WaterRenderSystem> {
  public:
    WaterRenderSystem();
    ~WaterRenderSystem() override;

    void init(fabric::AppContext& ctx) override;
    void render(fabric::AppContext& ctx) override;
    void shutdown() override;
    void configureDependencies() override;

  private:
    // Seed water field for a single chunk region using TerrainGenerator
    void seedWaterForChunk(int cx, int cy, int cz);

    // Rebuild the water mesh for a single chunk, returns true if non-empty
    bool rebuildWaterMesh(int cx, int cy, int cz);

    fabric::FieldLayer<float> waterField_;
    std::unique_ptr<recurse::WaterSimulation> waterSim_;
    recurse::WaterRenderer waterRenderer_;

    // Per-chunk water mesh cache, keyed by chunk coordinate
    std::unordered_map<ChunkCoord, WaterChunkMesh, ChunkCoordHash> waterMeshes_;

    // Chunks that need water mesh rebuild (seeded by WaterSimulation change events)
    std::unordered_set<ChunkCoord, ChunkCoordHash> dirtyWaterChunks_;

    // Cached system pointers (set during init)
    TerrainSystem* terrain_ = nullptr;
    ChunkPipelineSystem* chunks_ = nullptr;
    ShadowRenderSystem* shadow_ = nullptr;

    // Budget: max water mesh rebuilds per frame
    static constexpr int kMaxWaterMeshRebuildsPerFrame = 4;
};

} // namespace recurse::systems
