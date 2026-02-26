#include "fabric/core/VoxelRenderer.hh"

#include "fabric/core/ChunkedGrid.hh"
#include "fabric/core/Log.hh"
#include "fabric/utils/Profiler.hh"

#include <bx/math.h>

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
    : program_(BGFX_INVALID_HANDLE), uniformPalette_(BGFX_INVALID_HANDLE), uniformLightDir_(BGFX_INVALID_HANDLE) {}

VoxelRenderer::~VoxelRenderer() {
    shutdown();
}

void VoxelRenderer::shutdown() {
    if (bgfx::isValid(uniformLightDir_)) {
        bgfx::destroy(uniformLightDir_);
    }
    if (bgfx::isValid(uniformPalette_)) {
        bgfx::destroy(uniformPalette_);
    }
    if (bgfx::isValid(program_)) {
        bgfx::destroy(program_);
    }

    uniformLightDir_ = BGFX_INVALID_HANDLE;
    uniformPalette_ = BGFX_INVALID_HANDLE;
    program_ = BGFX_INVALID_HANDLE;
    initialized_ = false;
}

void VoxelRenderer::initProgram() {
    bgfx::RendererType::Enum type = bgfx::getRendererType();
    program_ = bgfx::createProgram(bgfx::createEmbeddedShader(s_voxelShaders, type, "vs_voxel"),
                                   bgfx::createEmbeddedShader(s_voxelShaders, type, "fs_voxel"), true);

    uniformPalette_ = bgfx::createUniform("u_palette", bgfx::UniformType::Vec4, 256);
    uniformLightDir_ = bgfx::createUniform("u_lightDir", bgfx::UniformType::Vec4);

    if (!bgfx::isValid(program_) || !bgfx::isValid(uniformPalette_) || !bgfx::isValid(uniformLightDir_)) {
        FABRIC_LOG_ERROR("VoxelRenderer shader/uniform init failed for renderer {}", bgfx::getRendererName(type));
        shutdown();
        return;
    }

    initialized_ = true;
    FABRIC_LOG_INFO("VoxelRenderer shader program initialized");
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

} // namespace fabric
