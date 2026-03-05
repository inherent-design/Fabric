#pragma once

#include "recurse/world/GradientNormals.hh"
#include "recurse/world/MesherInterface.hh"

namespace recurse {

class SnapMCMesher : public MesherInterface {
  public:
    struct Config {
        float snapEpsilon = 0.05f; // 5% of edge length - conservative snapping to preserve sharp features
    };

    SnapMCMesher();
    explicit SnapMCMesher(const Config& config);

    SmoothChunkMeshData meshChunk(const ChunkDensityCache& density, const ChunkMaterialCache& material,
                                  float isovalue = 0.5f, int lodLevel = 0) override;

    const char* name() const override { return "SnapMC"; }

  private:
    Config config_;
};

} // namespace recurse
