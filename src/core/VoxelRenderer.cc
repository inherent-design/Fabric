#include "fabric/core/VoxelRenderer.hh"

#include "fabric/core/ChunkedGrid.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/Rendering.hh"
#include "fabric/utils/Profiler.hh"

#include <algorithm>
#include <bx/math.h>
#include <vector>

// Suppress shader profiles we don't compile per-platform.
#if !defined(_WIN32)
#define BGFX_PLATFORM_SUPPORTS_DXBC 0
#endif
#define BGFX_PLATFORM_SUPPORTS_DXIL 0
#define BGFX_PLATFORM_SUPPORTS_WGSL 0
#include <bgfx/embedded_shader.h>

// Compiled shader bytecode generated at build time from .sc sources.
#include "essl/fs_voxel.sc.bin.h"
#include "essl/vs_voxel.sc.bin.h"
#include "glsl/fs_voxel.sc.bin.h"
#include "glsl/vs_voxel.sc.bin.h"
#include "spv/fs_voxel.sc.bin.h"
#include "spv/vs_voxel.sc.bin.h"
#if BX_PLATFORM_WINDOWS
#include "dxbc/fs_voxel.sc.bin.h"
#include "dxbc/vs_voxel.sc.bin.h"
#endif
#if BX_PLATFORM_OSX || BX_PLATFORM_IOS || BX_PLATFORM_VISIONOS
#include "mtl/fs_voxel.sc.bin.h"
#include "mtl/vs_voxel.sc.bin.h"
#endif

static const bgfx::EmbeddedShader s_voxelShaders[] = {BGFX_EMBEDDED_SHADER(vs_voxel), BGFX_EMBEDDED_SHADER(fs_voxel),
                                                      BGFX_EMBEDDED_SHADER_END()};

namespace fabric {

VoxelRenderer::VoxelRenderer()
    : program_(BGFX_INVALID_HANDLE),
      uniformPalette_(BGFX_INVALID_HANDLE),
      uniformLightDir_(BGFX_INVALID_HANDLE),
      indirectBuffer_(BGFX_INVALID_HANDLE) {}

VoxelRenderer::~VoxelRenderer() {
    shutdown();
}

void VoxelRenderer::shutdown() {
    if (bgfx::isValid(indirectBuffer_)) {
        bgfx::destroy(indirectBuffer_);
    }
    if (bgfx::isValid(uniformLightDir_)) {
        bgfx::destroy(uniformLightDir_);
    }
    if (bgfx::isValid(uniformPalette_)) {
        bgfx::destroy(uniformPalette_);
    }
    if (bgfx::isValid(program_)) {
        bgfx::destroy(program_);
    }

    indirectBuffer_ = BGFX_INVALID_HANDLE;
    uniformLightDir_ = BGFX_INVALID_HANDLE;
    uniformPalette_ = BGFX_INVALID_HANDLE;
    program_ = BGFX_INVALID_HANDLE;
    initialized_ = false;
    mdiSupported_ = false;
}

void VoxelRenderer::initProgram() {
    bgfx::RendererType::Enum type = bgfx::getRendererType();
    program_ = bgfx::createProgram(bgfx::createEmbeddedShader(s_voxelShaders, type, "vs_voxel"),
                                   bgfx::createEmbeddedShader(s_voxelShaders, type, "fs_voxel"), true);

    uniformPalette_ = bgfx::createUniform("u_palette", bgfx::UniformType::Vec4, 256);
    uniformLightDir_ = bgfx::createUniform("u_lightDir", bgfx::UniformType::Vec4);

    if (!bgfx::isValid(program_) || !bgfx::isValid(uniformPalette_) || !bgfx::isValid(uniformLightDir_)) {
        FABRIC_LOG_RENDER_ERROR("VoxelRenderer shader/uniform init failed for renderer {}",
                                bgfx::getRendererName(type));
        shutdown();
        return;
    }

    initialized_ = true;

    const auto& caps = renderCaps();
    FABRIC_LOG_RENDER_INFO("VoxelRenderer initialized: backend={}, tier={}", caps.rendererName,
                           static_cast<int>(caps.tier));
    if (!caps.index32) {
        FABRIC_LOG_RENDER_WARN("VoxelRenderer: 32-bit indices unavailable; chunk meshes limited to 65535 vertices");
    }

    mdiSupported_ = caps.drawIndirect;
    if (mdiSupported_) {
        indirectBuffer_ = bgfx::createIndirectBuffer(kMaxIndirectDraws);
        if (!bgfx::isValid(indirectBuffer_)) {
            FABRIC_LOG_RENDER_WARN("VoxelRenderer: indirect buffer allocation failed, MDI disabled");
            mdiSupported_ = false;
        } else {
            FABRIC_LOG_RENDER_INFO("VoxelRenderer: MDI enabled ({} max draws)", kMaxIndirectDraws);
        }
    } else {
        FABRIC_LOG_RENDER_INFO("VoxelRenderer: MDI unavailable, per-chunk submit");
    }
}

void VoxelRenderer::render(bgfx::ViewId view, const ChunkMesh& mesh, int chunkX, int chunkY, int chunkZ) {
    FABRIC_ZONE_SCOPED;

    if (!mesh.valid || mesh.indexCount == 0) {
        return;
    }

    if (!initialized_) {
        initProgram();
    }

    if (!isValid() || !bgfx::isValid(mesh.vbh) || !bgfx::isValid(mesh.ibh)) {
        return;
    }

    // Chunk world-space transform: translate by chunk coordinates * chunk size
    float mtx[16];
    bx::mtxIdentity(mtx);
    mtx[12] = static_cast<float>(chunkX * kChunkSize);
    mtx[13] = static_cast<float>(chunkY * kChunkSize);
    mtx[14] = static_cast<float>(chunkZ * kChunkSize);
    bgfx::setTransform(mtx);

    // Upload palette colors (up to 256 entries)
    if (!mesh.palette.empty()) {
        auto count = static_cast<uint16_t>(std::min(mesh.palette.size(), static_cast<size_t>(256)));
        bgfx::setUniform(uniformPalette_, mesh.palette.data(), count);
    }

    bgfx::setVertexBuffer(0, mesh.vbh);
    bgfx::setIndexBuffer(mesh.ibh);

    uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS |
                     BGFX_STATE_MSAA | BGFX_STATE_CULL_CCW;
    bgfx::setState(state);

    bgfx::submit(view, program_);
}

void VoxelRenderer::setLightDirection(const Vector3<float, Space::World>& dir) {
    if (!initialized_) {
        initProgram();
    }

    if (!isValid() || !bgfx::isValid(uniformLightDir_)) {
        return;
    }

    float lightDir[4] = {dir.x, dir.y, dir.z, 0.0f};
    bgfx::setUniform(uniformLightDir_, lightDir);
}

bool VoxelRenderer::isValid() const {
    return bgfx::isValid(program_);
}

bool VoxelRenderer::mdiSupported() const {
    return mdiSupported_;
}

void VoxelRenderer::renderBatch(bgfx::ViewId view, const ChunkRenderInfo* chunks, uint32_t count) {
    FABRIC_ZONE_SCOPED;

    if (count == 0) {
        return;
    }

    if (!initialized_) {
        initProgram();
    }

    if (!isValid()) {
        return;
    }

    if (mdiSupported_ && count > 1) {
        renderIndirect(view, chunks, count);
    } else {
        for (uint32_t i = 0; i < count; ++i) {
            const auto& ci = chunks[i];
            render(view, *ci.mesh, ci.chunkX, ci.chunkY, ci.chunkZ);
        }
    }
}

void VoxelRenderer::renderIndirect(bgfx::ViewId view, const ChunkRenderInfo* chunks, uint32_t count) {
    FABRIC_ZONE_SCOPED_N("VoxelRenderer::renderIndirect");

    // Group chunks by palette to minimize uniform uploads.
    // Within a group, selective discard flags preserve palette uniform
    // and render state across submits (only transform/VBO/IBO reset).
    // True single-submit MDI requires VertexPool (shared mega-buffer).

    struct PaletteGroup {
        const std::vector<std::array<float, 4>>* palette;
        std::vector<uint32_t> indices;
    };

    std::vector<PaletteGroup> groups;
    groups.reserve(4); // typical palette count

    for (uint32_t i = 0; i < count; ++i) {
        const auto& ci = chunks[i];
        if (!ci.mesh || !ci.mesh->valid || ci.mesh->indexCount == 0) {
            continue;
        }
        if (!bgfx::isValid(ci.mesh->vbh) || !bgfx::isValid(ci.mesh->ibh)) {
            continue;
        }

        bool matched = false;
        for (auto& g : groups) {
            if (*g.palette == ci.mesh->palette) {
                g.indices.push_back(i);
                matched = true;
                break;
            }
        }
        if (!matched) {
            groups.push_back({&ci.mesh->palette, {i}});
        }
    }

    uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS |
                     BGFX_STATE_MSAA | BGFX_STATE_CULL_CCW;

    // Discard per-chunk resources; preserve uniforms + render state
    constexpr uint8_t kGroupDiscard =
        BGFX_DISCARD_INDEX_BUFFER | BGFX_DISCARD_INSTANCE_DATA | BGFX_DISCARD_TRANSFORM | BGFX_DISCARD_VERTEX_STREAMS;

    for (const auto& group : groups) {
        if (!group.palette->empty()) {
            auto cnt = static_cast<uint16_t>(std::min(group.palette->size(), static_cast<size_t>(256)));
            bgfx::setUniform(uniformPalette_, group.palette->data(), cnt);
        }
        bgfx::setState(state);

        for (size_t j = 0; j < group.indices.size(); ++j) {
            const auto& ci = chunks[group.indices[j]];

            float mtx[16];
            bx::mtxIdentity(mtx);
            mtx[12] = static_cast<float>(ci.chunkX * kChunkSize);
            mtx[13] = static_cast<float>(ci.chunkY * kChunkSize);
            mtx[14] = static_cast<float>(ci.chunkZ * kChunkSize);
            bgfx::setTransform(mtx);

            bgfx::setVertexBuffer(0, ci.mesh->vbh);
            bgfx::setIndexBuffer(ci.mesh->ibh);

            bool last = (j + 1 == group.indices.size());
            bgfx::submit(view, program_, 0, last ? BGFX_DISCARD_ALL : kGroupDiscard);
        }
    }
}

} // namespace fabric
