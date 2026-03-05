#pragma once

#include "recurse/world/GradientNormals.hh"
#include "recurse/world/MesherInterface.hh"

namespace recurse {

class SurfaceNetsMesher : public MesherInterface {
  public:
    SmoothChunkMeshData meshChunk(const ChunkDensityCache& density, const ChunkMaterialCache& material,
                                  float isovalue = 0.5f, int lodLevel = 0) override;

    const char* name() const override { return "SurfaceNets"; }
};

} // namespace recurse
