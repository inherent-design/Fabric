#include "recurse/animation/SkinnedRenderer.hh"

#include "fabric/core/Log.hh"
#include "fabric/utils/ErrorHandling.hh"
#include "fabric/utils/Profiler.hh"

// Vulkan-only: suppress all non-SPIR-V shader profiles so
// BGFX_EMBEDDED_SHADER only references *_spv symbol arrays.
#define BGFX_PLATFORM_SUPPORTS_DXBC 0
#define BGFX_PLATFORM_SUPPORTS_DXIL 0
#define BGFX_PLATFORM_SUPPORTS_ESSL 0
#define BGFX_PLATFORM_SUPPORTS_GLSL 0
#define BGFX_PLATFORM_SUPPORTS_METAL 0
#define BGFX_PLATFORM_SUPPORTS_NVN 0
#define BGFX_PLATFORM_SUPPORTS_PSSL 0
#define BGFX_PLATFORM_SUPPORTS_WGSL 0
#include <bgfx/embedded_shader.h>

// Compiled SPIR-V shader bytecode generated at build time from .sc sources.
#include "spv/fs_skinned.sc.bin.h"
#include "spv/vs_skinned.sc.bin.h"

static const bgfx::EmbeddedShader s_skinnedShaders[] = {BGFX_EMBEDDED_SHADER(vs_skinned),
                                                        BGFX_EMBEDDED_SHADER(fs_skinned), BGFX_EMBEDDED_SHADER_END()};

using namespace fabric;

namespace recurse {

bgfx::VertexLayout createSkinnedVertexLayout() {
    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Indices, 4, bgfx::AttribType::Uint8)
        .add(bgfx::Attrib::Weight, 4, bgfx::AttribType::Float)
        .end();
    return layout;
}

SkinnedRenderer::SkinnedRenderer() : layout_(createSkinnedVertexLayout()) {}

SkinnedRenderer::~SkinnedRenderer() {
    meshBufferCache_.clear();
    uniformJointMatrices_.reset();
    program_.reset();
}

void SkinnedRenderer::render(bgfx::ViewId view, const MeshData& mesh, const SkinningData& skinning,
                             const Matrix4x4<float>& transform) {
    FABRIC_ZONE_SCOPED;

    // Lazy-init program on first render (requires bgfx to be active)
    if (!program_.isValid()) {
        bgfx::RendererType::Enum type = bgfx::getRendererType();
        program_.reset(bgfx::createProgram(bgfx::createEmbeddedShader(s_skinnedShaders, type, "vs_skinned"),
                                           bgfx::createEmbeddedShader(s_skinnedShaders, type, "fs_skinned"), true));

        uniformJointMatrices_.reset(bgfx::createUniform("u_jointMatrices", bgfx::UniformType::Mat4, kMaxGpuJoints));

        FABRIC_LOG_INFO("SkinnedRenderer shader program initialized");
    }

    if (mesh.positions.empty() || mesh.indices.empty()) {
        return;
    }

    const auto cacheKey = mesh.id;
    auto it = meshBufferCache_.find(cacheKey);

    if (it == meshBufferCache_.end()) {
        // Cache miss: pack vertex data and create static buffers
        const size_t vertexCount = mesh.positions.size();
        const size_t stride = layout_.getStride();
        std::vector<uint8_t> vertexData(vertexCount * stride);

        for (size_t i = 0; i < vertexCount; ++i) {
            auto* dst = vertexData.data() + i * stride;
            size_t offset = 0;

            // Position (3 floats)
            const auto& pos = mesh.positions[i];
            std::memcpy(dst + offset, &pos.x, sizeof(float) * 3);
            offset += sizeof(float) * 3;

            // Normal (3 floats)
            if (i < mesh.normals.size()) {
                const auto& n = mesh.normals[i];
                std::memcpy(dst + offset, &n.x, sizeof(float) * 3);
            } else {
                float defaultNormal[3] = {0.0f, 1.0f, 0.0f};
                std::memcpy(dst + offset, defaultNormal, sizeof(float) * 3);
            }
            offset += sizeof(float) * 3;

            // UV (2 floats)
            if (i < mesh.uvs.size()) {
                const auto& uv = mesh.uvs[i];
                std::memcpy(dst + offset, &uv.x, sizeof(float) * 2);
            } else {
                float defaultUv[2] = {0.0f, 0.0f};
                std::memcpy(dst + offset, defaultUv, sizeof(float) * 2);
            }
            offset += sizeof(float) * 2;

            // Joint indices (4 uint8)
            if (i < mesh.jointIndices.size()) {
                const auto& joints = mesh.jointIndices[i];
                uint8_t packed[4] = {static_cast<uint8_t>(joints[0]), static_cast<uint8_t>(joints[1]),
                                     static_cast<uint8_t>(joints[2]), static_cast<uint8_t>(joints[3])};
                std::memcpy(dst + offset, packed, 4);
            } else {
                uint8_t zero[4] = {0, 0, 0, 0};
                std::memcpy(dst + offset, zero, 4);
            }
            offset += 4;

            // Weights (4 floats)
            if (i < mesh.jointWeights.size()) {
                const auto& w = mesh.jointWeights[i];
                std::memcpy(dst + offset, &w.x, sizeof(float) * 4);
            } else {
                float defaultWeight[4] = {1.0f, 0.0f, 0.0f, 0.0f};
                std::memcpy(dst + offset, defaultWeight, sizeof(float) * 4);
            }
        }

        MeshBufferCache cache;
        cache.vbh.reset(
            bgfx::createVertexBuffer(bgfx::copy(vertexData.data(), static_cast<uint32_t>(vertexData.size())), layout_));
        cache.ibh.reset(bgfx::createIndexBuffer(
            bgfx::copy(mesh.indices.data(), static_cast<uint32_t>(mesh.indices.size() * sizeof(uint32_t))),
            BGFX_BUFFER_INDEX32));
        cache.vertexCount = vertexCount;
        cache.indexCount = mesh.indices.size();

        it = meshBufferCache_.emplace(cacheKey, std::move(cache)).first;
    }

    const auto& cache = it->second;

    // Upload joint matrices (clamp to max GPU joints)
    const size_t jointCount = std::min(skinning.jointMatrices.size(), static_cast<size_t>(kMaxGpuJoints));
    if (jointCount > 0) {
        bgfx::setUniform(uniformJointMatrices_.get(), skinning.jointMatrices[0].data(),
                         static_cast<uint16_t>(jointCount));
    }

    // Transpose column-major Matrix4x4 to row-major for bgfx
    float mtx[16];
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            mtx[r * 4 + c] = transform.elements[c * 4 + r];
    bgfx::setTransform(mtx);

    // Set cached static buffers and submit
    bgfx::setVertexBuffer(0, cache.vbh.get());
    bgfx::setIndexBuffer(cache.ibh.get());

    uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS |
                     BGFX_STATE_MSAA | BGFX_STATE_CULL_CCW;
    bgfx::setState(state);

    bgfx::submit(view, program_.get());
}

const bgfx::VertexLayout& SkinnedRenderer::vertexLayout() const {
    return layout_;
}

bool SkinnedRenderer::isValid() const {
    return program_.isValid();
}

} // namespace recurse
