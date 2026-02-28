#pragma once

#include "fabric/core/Spatial.hh"
#include "fabric/core/VoxelMesher.hh"
#include <bgfx/bgfx.h>
#include <cstdint>

namespace fabric {

// Renders water chunk meshes with animated UV scrolling, transparency,
// and Fresnel-based sky tinting.
//
// Consumes WaterChunkMesh geometry produced by VoxelMesher::meshWaterChunk().
// Submits with alpha blending enabled and depth-write OFF so that solid
// geometry behind water remains visible.
//
// Uniforms:
//   u_waterColor (vec4) - base water color (RGB) + alpha (A)
//   u_time       (vec4) - x = elapsed seconds, yzw unused
//   u_lightDir   (vec4) - normalized direction toward the light source
//
// Requires bgfx to be initialized before first render call; shader source
// is embedded at compile time via the bgfx shader compiler (offline).
class WaterRenderer {
  public:
    WaterRenderer();
    ~WaterRenderer();

    void shutdown();

    WaterRenderer(const WaterRenderer&) = delete;
    WaterRenderer& operator=(const WaterRenderer&) = delete;

    // Render a single water chunk mesh at the given chunk coordinates.
    void render(bgfx::ViewId view, const WaterChunkMesh& mesh, int chunkX, int chunkY, int chunkZ);

    // Set the base water color (RGBA). Alpha controls translucency.
    void setWaterColor(float r, float g, float b, float a);

    // Set the elapsed time in seconds (drives UV scrolling animation).
    void setTime(float seconds);

    // Set the directional light direction (normalized, pointing toward light).
    void setLightDirection(const Vector3<float, Space::World>& dir);

    // Check if the shader program handle is valid.
    bool isValid() const;

  private:
    void initProgram();

    bgfx::ProgramHandle program_;
    bgfx::UniformHandle uniformWaterColor_;
    bgfx::UniformHandle uniformTime_;
    bgfx::UniformHandle uniformLightDir_;
    bool initialized_ = false;

    float waterColor_[4] = {0.1f, 0.3f, 0.6f, 0.7f};
    float time_[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float lightDir_[4] = {0.3f, 0.8f, 0.5f, 0.0f};
};

} // namespace fabric
