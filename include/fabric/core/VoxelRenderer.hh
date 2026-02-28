#pragma once

#include "fabric/core/RenderCaps.hh"
#include "fabric/core/Spatial.hh"
#include "fabric/core/VoxelMesher.hh"
#include <bgfx/bgfx.h>
#include <optional>

namespace fabric {

// Renders voxel chunks using the voxel shader program.
// Manages the bgfx shader program, palette uniform, and light direction uniform.
// Requires bgfx to be initialized before first render call; shader source is
// embedded at compile time via the bgfx shader compiler (offline).
class VoxelRenderer {
  public:
    VoxelRenderer();
    ~VoxelRenderer();

    void shutdown();

    VoxelRenderer(const VoxelRenderer&) = delete;
    VoxelRenderer& operator=(const VoxelRenderer&) = delete;

    // Render a single chunk mesh at the given chunk coordinates.
    // The chunk position is converted to world-space transform internally.
    void render(bgfx::ViewId view, const ChunkMesh& mesh, int chunkX, int chunkY, int chunkZ);

    // Set the directional light direction (normalized, pointing toward light).
    void setLightDirection(const Vector3<float, Space::World>& dir);

    // Check if the shader program handle is valid
    bool isValid() const;

    // Access detected render capabilities (valid after first render/init)
    const RenderCaps* caps() const;

  private:
    void initProgram();

    bgfx::ProgramHandle program_;
    bgfx::UniformHandle uniformPalette_;
    bgfx::UniformHandle uniformLightDir_;
    std::optional<RenderCaps> caps_;
    bool initialized_ = false;
};

} // namespace fabric
