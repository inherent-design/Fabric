#pragma once

#include "recurse/world/GradientNormals.hh"
#include "recurse/world/MesherInterface.hh"

namespace recurse {

/// Reference Surface Nets smooth mesher.
///
/// This implementation remains useful for experimentation and parity checks
/// while the current Goal #4 plus meshing rollout proceeds through Checkpoints
/// 0-4 and keeps the production near-chunk path Greedy-first.
class SurfaceNetsMesher : public MesherInterface {
  public:
    SmoothChunkMeshData meshChunk(const ChunkDensityCache& density, const ChunkMaterialCache& material,
                                  float isovalue = 0.5f, int lodLevel = 0) override;

    const char* name() const override { return "SurfaceNets"; }
};

} // namespace recurse
