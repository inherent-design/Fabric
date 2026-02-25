#include "fabric/core/SkinnedRenderer.hh"

#include "fabric/core/Log.hh"
#include "fabric/utils/ErrorHandling.hh"
#include "fabric/utils/Profiler.hh"

// Suppress shader profiles we don't compile per-platform.
#if !defined(_WIN32)
#define BGFX_PLATFORM_SUPPORTS_DXBC 0
#endif
#define BGFX_PLATFORM_SUPPORTS_DXIL 0
#define BGFX_PLATFORM_SUPPORTS_WGSL 0
#include <bgfx/embedded_shader.h>

// Compiled shader bytecode generated at build time from .sc sources.
#include "essl/fs_skinned.sc.bin.h"
#include "essl/vs_skinned.sc.bin.h"
#include "glsl/fs_skinned.sc.bin.h"
#include "glsl/vs_skinned.sc.bin.h"
#include "spv/fs_skinned.sc.bin.h"
#include "spv/vs_skinned.sc.bin.h"
#if BX_PLATFORM_WINDOWS
#include "dxbc/fs_skinned.sc.bin.h"
#include "dxbc/vs_skinned.sc.bin.h"
#endif
#if BX_PLATFORM_OSX || BX_PLATFORM_IOS || BX_PLATFORM_VISIONOS
#include "mtl/fs_skinned.sc.bin.h"
#include "mtl/vs_skinned.sc.bin.h"
#endif

static const bgfx::EmbeddedShader s_skinnedShaders[] = {BGFX_EMBEDDED_SHADER(vs_skinned),
                                                        BGFX_EMBEDDED_SHADER(fs_skinned), BGFX_EMBEDDED_SHADER_END()};

namespace fabric {

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

SkinnedRenderer::SkinnedRenderer()
    : layout_(createSkinnedVertexLayout()), program_(BGFX_INVALID_HANDLE), uniformJointMatrices_(BGFX_INVALID_HANDLE) {}

SkinnedRenderer::~SkinnedRenderer() {
    for (auto& [_, cache] : meshBufferCache_) {
        if (bgfx::isValid(cache.vbh)) {
            bgfx::destroy(cache.vbh);
        }
        if (bgfx::isValid(cache.ibh)) {
            bgfx::destroy(cache.ibh);
        }
    }
    if (bgfx::isValid(uniformJointMatrices_)) {
        bgfx::destroy(uniformJointMatrices_);
    }
    if (bgfx::isValid(program_)) {
        bgfx::destroy(program_);
    }
}

void SkinnedRenderer::render(bgfx::ViewId view, const MeshData& mesh, const SkinningData& skinning,
                             const Matrix4x4<float>& transform) {
    FABRIC_ZONE_SCOPED;

    // Lazy-init program on first render (requires bgfx to be active)
    if (!bgfx::isValid(program_)) {
        bgfx::RendererType::Enum type = bgfx::getRendererType();
        program_ = bgfx::createProgram(bgfx::createEmbeddedShader(s_skinnedShaders, type, "vs_skinned"),
                                       bgfx::createEmbeddedShader(s_skinnedShaders, type, "fs_skinned"), true);

        uniformJointMatrices_ = bgfx::createUniform("u_jointMatrices", bgfx::UniformType::Mat4, kMaxGpuJoints);

        FABRIC_LOG_INFO("SkinnedRenderer shader program initialized");
    }

    if (mesh.positions.empty() || mesh.indices.empty()) {
        return;
    }

    const auto* cacheKey = static_cast<const void*>(&mesh);
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
        cache.vbh =
            bgfx::createVertexBuffer(bgfx::copy(vertexData.data(), static_cast<uint32_t>(vertexData.size())), layout_);
        cache.ibh = bgfx::createIndexBuffer(
            bgfx::copy(mesh.indices.data(), static_cast<uint32_t>(mesh.indices.size() * sizeof(uint32_t))),
            BGFX_BUFFER_INDEX32);
        cache.vertexCount = vertexCount;
        cache.indexCount = mesh.indices.size();

        it = meshBufferCache_.emplace(cacheKey, cache).first;
    }

    const auto& cache = it->second;

    // Upload joint matrices (clamp to max GPU joints)
    const size_t jointCount = std::min(skinning.jointMatrices.size(), static_cast<size_t>(kMaxGpuJoints));
    if (jointCount > 0) {
        bgfx::setUniform(uniformJointMatrices_, skinning.jointMatrices[0].data(), static_cast<uint16_t>(jointCount));
    }

    // Set transform
    bgfx::setTransform(transform.elements.data());

    // Set cached static buffers and submit
    bgfx::setVertexBuffer(0, cache.vbh);
    bgfx::setIndexBuffer(cache.ibh);

    uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS |
                     BGFX_STATE_MSAA | BGFX_STATE_CULL_CCW;
    bgfx::setState(state);

    bgfx::submit(view, program_);
}

const bgfx::VertexLayout& SkinnedRenderer::vertexLayout() const {
    return layout_;
}

bool SkinnedRenderer::isValid() const {
    return bgfx::isValid(program_);
}

} // namespace fabric
