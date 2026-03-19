#pragma once

#include "recurse/world/GradientNormals.hh"
#include "recurse/world/MesherInterface.hh"

namespace recurse {

/// Experimental SnapMC smooth mesher.
///
/// Kept available behind MesherInterface for comparison, rollback, and
/// research. It is not the primary production near-chunk path.
class SnapMCMesher : public MesherInterface {
  public:
    /// Tuning parameters for the experimental SnapMC implementation.
    struct Config {
        float snapEpsilon = 0.05f; // 5% of edge length; conservative snapping preserves sharp features
    };

    /// Construct with default experimental settings.
    SnapMCMesher();

    /// Construct with an explicit experimental configuration.
    explicit SnapMCMesher(const Config& config);

    SmoothChunkMeshData meshChunk(const ChunkDensityCache& density, const ChunkMaterialCache& material,
                                  float isovalue = 0.5f, int lodLevel = 0) override;

    const char* name() const override { return "SnapMC"; }

  private:
    Config config_;
};

} // namespace recurse
