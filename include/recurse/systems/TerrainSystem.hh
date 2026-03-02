#pragma once

#include "fabric/core/FieldLayer.hh"
#include "fabric/core/Spatial.hh"
#include "fabric/core/SystemBase.hh"
#include <memory>

namespace recurse {
class TerrainGenerator;
class CaveCarver;
struct TerrainConfig;
struct CaveConfig;
template <typename T> class ChunkedGrid;
} // namespace recurse

namespace recurse::systems {

/// Owns terrain density and essence fields, generator, and cave carver.
/// Init-only: fixedUpdate is a no-op. Terrain generation for individual
/// chunks is exposed via generateChunkTerrain() for use by ChunkPipelineSystem.
class TerrainSystem : public fabric::System<TerrainSystem> {
  public:
    TerrainSystem() = default;

    void init(fabric::AppContext& ctx) override;
    void fixedUpdate(fabric::AppContext& ctx, float fixedDt) override;

    void configureDependencies() override;

    // Generate terrain for a single chunk region (moved from anonymous helper)
    void generateChunkTerrain(int cx, int cy, int cz);

    // Accessors
    fabric::DensityField& density() { return density_; }
    const fabric::DensityField& density() const { return density_; }
    fabric::EssenceField& essence() { return essence_; }
    const fabric::EssenceField& essence() const { return essence_; }
    ChunkedGrid<float>& densityGrid() { return density_.grid(); }
    const ChunkedGrid<float>& densityGrid() const { return density_.grid(); }
    ChunkedGrid<fabric::Vector4<float, fabric::Space::World>>& essenceGrid() { return essence_.grid(); }
    const ChunkedGrid<fabric::Vector4<float, fabric::Space::World>>& essenceGrid() const { return essence_.grid(); }
    TerrainGenerator& terrainGen() { return *terrainGen_; }
    CaveCarver& caveCarver() { return *caveCarver_; }

  private:
    fabric::DensityField density_;
    fabric::EssenceField essence_;
    std::unique_ptr<TerrainGenerator> terrainGen_;
    std::unique_ptr<CaveCarver> caveCarver_;
};

} // namespace recurse::systems
