#pragma once

#include "fabric/core/Animation.hh"
#include "fabric/core/MeshLoader.hh"
#include "fabric/core/Spatial.hh"
#include <bgfx/bgfx.h>
#include <cstdint>
#include <unordered_map>

namespace fabric {

// Maximum joints uploadable to the GPU skinning shader uniform array.
// Derived from kMaxJoints in Animation.hh to keep a single source of truth.
inline constexpr int kMaxGpuJoints = kMaxJoints;

// bgfx vertex layout for skinned meshes:
// pos(float3) + normal(float3) + uv(float2) + joints(uint8x4) + weights(float4)
bgfx::VertexLayout createSkinnedVertexLayout();

// Renders skinned meshes using the GPU skinning shader.
// Manages the bgfx shader program, vertex layout, and joint matrix uniform.
// Requires bgfx to be initialized before construction; shader source is embedded
// at compile time via the bgfx shader compiler (offline).
class SkinnedRenderer {
  public:
    SkinnedRenderer();
    ~SkinnedRenderer();

    SkinnedRenderer(const SkinnedRenderer&) = delete;
    SkinnedRenderer& operator=(const SkinnedRenderer&) = delete;

    // Render a skinned mesh with the given joint matrices and world transform.
    // The MeshData must contain skinning data (jointIndices, jointWeights).
    // Joint matrices are uploaded as a uniform array (max 100 joints).
    void render(bgfx::ViewId view, const MeshData& mesh, const SkinningData& skinning,
                const Matrix4x4<float>& transform);

    // Access the vertex layout for external buffer creation
    const bgfx::VertexLayout& vertexLayout() const;

    // Check if the shader program handle is valid
    bool isValid() const;

  private:
    bgfx::VertexLayout layout_;
    bgfx::ProgramHandle program_;
    bgfx::UniformHandle uniformJointMatrices_;

    // Cache: static vertex/index buffers per mesh pointer (geometry is immutable)
    struct MeshBufferCache {
        bgfx::VertexBufferHandle vbh = BGFX_INVALID_HANDLE;
        bgfx::IndexBufferHandle ibh = BGFX_INVALID_HANDLE;
        size_t vertexCount = 0;
        size_t indexCount = 0;
    };
    std::unordered_map<const void*, MeshBufferCache> meshBufferCache_;
};

} // namespace fabric
