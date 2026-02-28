#include "fabric/core/WaterRenderer.hh"

#include "fabric/core/ChunkedGrid.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/Rendering.hh"
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
#include "essl/fs_water.sc.bin.h"
#include "essl/vs_water.sc.bin.h"
#include "glsl/fs_water.sc.bin.h"
#include "glsl/vs_water.sc.bin.h"
#include "spv/fs_water.sc.bin.h"
#include "spv/vs_water.sc.bin.h"
#if BX_PLATFORM_WINDOWS
#include "dxbc/fs_water.sc.bin.h"
#include "dxbc/vs_water.sc.bin.h"
#endif
#if BX_PLATFORM_OSX || BX_PLATFORM_IOS || BX_PLATFORM_VISIONOS
#include "mtl/fs_water.sc.bin.h"
#include "mtl/vs_water.sc.bin.h"
#endif

static const bgfx::EmbeddedShader s_waterShaders[] = {BGFX_EMBEDDED_SHADER(vs_water), BGFX_EMBEDDED_SHADER(fs_water),
                                                      BGFX_EMBEDDED_SHADER_END()};

namespace fabric {

WaterRenderer::WaterRenderer()
    : program_(BGFX_INVALID_HANDLE),
      uniformWaterColor_(BGFX_INVALID_HANDLE),
      uniformTime_(BGFX_INVALID_HANDLE),
      uniformLightDir_(BGFX_INVALID_HANDLE) {}

WaterRenderer::~WaterRenderer() {
    shutdown();
}

void WaterRenderer::shutdown() {
    if (bgfx::isValid(uniformLightDir_)) {
        bgfx::destroy(uniformLightDir_);
    }
    if (bgfx::isValid(uniformTime_)) {
        bgfx::destroy(uniformTime_);
    }
    if (bgfx::isValid(uniformWaterColor_)) {
        bgfx::destroy(uniformWaterColor_);
    }
    if (bgfx::isValid(program_)) {
        bgfx::destroy(program_);
    }

    uniformLightDir_ = BGFX_INVALID_HANDLE;
    uniformTime_ = BGFX_INVALID_HANDLE;
    uniformWaterColor_ = BGFX_INVALID_HANDLE;
    program_ = BGFX_INVALID_HANDLE;
    initialized_ = false;
}

void WaterRenderer::initProgram() {
    bgfx::RendererType::Enum type = bgfx::getRendererType();
    program_ = bgfx::createProgram(bgfx::createEmbeddedShader(s_waterShaders, type, "vs_water"),
                                   bgfx::createEmbeddedShader(s_waterShaders, type, "fs_water"), true);

    uniformWaterColor_ = bgfx::createUniform("u_waterColor", bgfx::UniformType::Vec4);
    uniformTime_ = bgfx::createUniform("u_time", bgfx::UniformType::Vec4);
    uniformLightDir_ = bgfx::createUniform("u_lightDir", bgfx::UniformType::Vec4);

    if (!bgfx::isValid(program_) || !bgfx::isValid(uniformWaterColor_) || !bgfx::isValid(uniformTime_) ||
        !bgfx::isValid(uniformLightDir_)) {
        FABRIC_LOG_ERROR("WaterRenderer shader/uniform init failed for renderer {}", bgfx::getRendererName(type));
        shutdown();
        return;
    }

    initialized_ = true;

    const auto& caps = renderCaps();
    FABRIC_LOG_INFO("WaterRenderer initialized: backend={}, tier={}", caps.rendererName, static_cast<int>(caps.tier));
}

void WaterRenderer::render(bgfx::ViewId view, const WaterChunkMesh& mesh, int chunkX, int chunkY, int chunkZ) {
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

    // Upload uniforms
    bgfx::setUniform(uniformWaterColor_, waterColor_);
    bgfx::setUniform(uniformTime_, time_);
    bgfx::setUniform(uniformLightDir_, lightDir_);

    bgfx::setVertexBuffer(0, mesh.vbh);
    bgfx::setIndexBuffer(mesh.ibh);

    // Alpha blending, depth test ON, depth write OFF (transparent surface)
    uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_MSAA |
                     BGFX_STATE_CULL_CCW | BGFX_STATE_BLEND_ALPHA;
    bgfx::setState(state);

    bgfx::submit(view, program_);
}

void WaterRenderer::setWaterColor(float r, float g, float b, float a) {
    waterColor_[0] = r;
    waterColor_[1] = g;
    waterColor_[2] = b;
    waterColor_[3] = a;
}

void WaterRenderer::setTime(float seconds) {
    time_[0] = seconds;
}

void WaterRenderer::setLightDirection(const Vector3<float, Space::World>& dir) {
    lightDir_[0] = dir.x;
    lightDir_[1] = dir.y;
    lightDir_[2] = dir.z;
    lightDir_[3] = 0.0f;
}

bool WaterRenderer::isValid() const {
    return bgfx::isValid(program_);
}

} // namespace fabric
