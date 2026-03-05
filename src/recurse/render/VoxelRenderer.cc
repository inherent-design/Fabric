#include "recurse/render/VoxelRenderer.hh"

#include "fabric/core/Log.hh"
#include "fabric/core/Rendering.hh"
#include "fabric/utils/Profiler.hh"

#include <algorithm>
#include <bx/math.h>
#include <vector>

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
#include "spv/fs_smooth.sc.bin.h"
#include "spv/vs_smooth.sc.bin.h"

static const bgfx::EmbeddedShader s_voxelShaders[] = {BGFX_EMBEDDED_SHADER(vs_smooth), BGFX_EMBEDDED_SHADER(fs_smooth),
                                                      BGFX_EMBEDDED_SHADER_END()};

using namespace fabric;

namespace recurse {

VoxelRenderer::VoxelRenderer()
    : program_(BGFX_INVALID_HANDLE),
      uniformPalette_(BGFX_INVALID_HANDLE),
      uniformLightDir_(BGFX_INVALID_HANDLE),
      uniformViewPos_(BGFX_INVALID_HANDLE),
      uniformLitColor_(BGFX_INVALID_HANDLE),
      uniformShadowColor_(BGFX_INVALID_HANDLE),
      uniformRimParams_(BGFX_INVALID_HANDLE),
      uniformOceanParams_(BGFX_INVALID_HANDLE),
      indirectBuffer_(BGFX_INVALID_HANDLE) {}

VoxelRenderer::~VoxelRenderer() {
    shutdown();
}

void VoxelRenderer::shutdown() {
    if (bgfx::isValid(indirectBuffer_)) {
        bgfx::destroy(indirectBuffer_);
    }
    if (bgfx::isValid(uniformOceanParams_)) {
        bgfx::destroy(uniformOceanParams_);
    }
    if (bgfx::isValid(uniformRimParams_)) {
        bgfx::destroy(uniformRimParams_);
    }
    if (bgfx::isValid(uniformShadowColor_)) {
        bgfx::destroy(uniformShadowColor_);
    }
    if (bgfx::isValid(uniformLitColor_)) {
        bgfx::destroy(uniformLitColor_);
    }
    if (bgfx::isValid(uniformViewPos_)) {
        bgfx::destroy(uniformViewPos_);
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
    uniformOceanParams_ = BGFX_INVALID_HANDLE;
    uniformRimParams_ = BGFX_INVALID_HANDLE;
    uniformShadowColor_ = BGFX_INVALID_HANDLE;
    uniformLitColor_ = BGFX_INVALID_HANDLE;
    uniformViewPos_ = BGFX_INVALID_HANDLE;
    uniformLightDir_ = BGFX_INVALID_HANDLE;
    uniformPalette_ = BGFX_INVALID_HANDLE;
    program_ = BGFX_INVALID_HANDLE;
    initialized_ = false;
    mdiSupported_ = false;
}

void VoxelRenderer::initProgram() {
    bgfx::RendererType::Enum type = bgfx::getRendererType();
    program_ = bgfx::createProgram(bgfx::createEmbeddedShader(s_voxelShaders, type, "vs_smooth"),
                                   bgfx::createEmbeddedShader(s_voxelShaders, type, "fs_smooth"), true);

    uniformPalette_ = bgfx::createUniform("u_palette", bgfx::UniformType::Vec4, 256);
    uniformLightDir_ = bgfx::createUniform("u_lightDir", bgfx::UniformType::Vec4);
    uniformViewPos_ = bgfx::createUniform("u_viewPos", bgfx::UniformType::Vec4);
    uniformLitColor_ = bgfx::createUniform("u_litColor", bgfx::UniformType::Vec4);
    uniformShadowColor_ = bgfx::createUniform("u_shadowColor", bgfx::UniformType::Vec4);
    uniformRimParams_ = bgfx::createUniform("u_rimParams", bgfx::UniformType::Vec4);
    uniformOceanParams_ = bgfx::createUniform("u_oceanParams", bgfx::UniformType::Vec4);

    if (!bgfx::isValid(program_) || !bgfx::isValid(uniformPalette_) || !bgfx::isValid(uniformLightDir_) ||
        !bgfx::isValid(uniformViewPos_) || !bgfx::isValid(uniformLitColor_) || !bgfx::isValid(uniformShadowColor_) ||
        !bgfx::isValid(uniformRimParams_) || !bgfx::isValid(uniformOceanParams_)) {
        FABRIC_LOG_RENDER_ERROR("VoxelRenderer shader/uniform init failed for renderer {}",
                                bgfx::getRendererName(type));
        shutdown();
        return;
    }

    constexpr float kLitColor[4] = {0.95f, 0.85f, 0.55f, 1.0f};
    constexpr float kShadowColor[4] = {0.45f, 0.35f, 0.55f, 1.0f};
    constexpr float kRimParams[4] = {3.0f, 0.15f, 0.0f, 0.0f};   // Reduced rim strength: 0.6 -> 0.3 -> 0.15
    constexpr float kOceanParams[4] = {16.0f, 0.2f, 0.0f, 0.0f}; // Reduced ocean specular: 0.8 -> 0.4 -> 0.2

    std::copy(std::begin(kLitColor), std::end(kLitColor), litColor_);
    std::copy(std::begin(kShadowColor), std::end(kShadowColor), shadowColor_);
    std::copy(std::begin(kRimParams), std::end(kRimParams), rimParams_);
    std::copy(std::begin(kOceanParams), std::end(kOceanParams), oceanParams_);

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

void VoxelRenderer::render(bgfx::ViewId view, const ChunkMesh& mesh, float offsetX, float offsetY, float offsetZ) {
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

    // Camera-relative transform supplied by caller.
    float mtx[16];
    bx::mtxIdentity(mtx);
    mtx[12] = offsetX;
    mtx[13] = offsetY;
    mtx[14] = offsetZ;
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
    bgfx::setUniform(uniformLightDir_, lightDir_);
    bgfx::setUniform(uniformViewPos_, viewPos_);
    bgfx::setUniform(uniformLitColor_, litColor_);
    bgfx::setUniform(uniformShadowColor_, shadowColor_);
    bgfx::setUniform(uniformRimParams_, rimParams_);
    bgfx::setUniform(uniformOceanParams_, oceanParams_);
    bgfx::submit(view, program_);
}

void VoxelRenderer::setLightDirection(const Vector3<float, Space::World>& dir) {
    lightDir_[0] = dir.x;
    lightDir_[1] = dir.y;
    lightDir_[2] = dir.z;
    lightDir_[3] = 0.0f;
}

void VoxelRenderer::setViewPosition(double x, double y, double z) {
    viewPos_[0] = static_cast<float>(x);
    viewPos_[1] = static_cast<float>(y);
    viewPos_[2] = static_cast<float>(z);
    viewPos_[3] = 1.0f;
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
            render(view, *ci.mesh, ci.offsetX, ci.offsetY, ci.offsetZ);
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
        bgfx::setUniform(uniformLightDir_, lightDir_);
        bgfx::setUniform(uniformViewPos_, viewPos_);
        bgfx::setUniform(uniformLitColor_, litColor_);
        bgfx::setUniform(uniformShadowColor_, shadowColor_);
        bgfx::setUniform(uniformRimParams_, rimParams_);
        bgfx::setUniform(uniformOceanParams_, oceanParams_);
        bgfx::setState(state);

        for (size_t j = 0; j < group.indices.size(); ++j) {
            const auto& ci = chunks[group.indices[j]];

            float mtx[16];
            bx::mtxIdentity(mtx);
            mtx[12] = ci.offsetX;
            mtx[13] = ci.offsetY;
            mtx[14] = ci.offsetZ;
            bgfx::setTransform(mtx);

            bgfx::setVertexBuffer(0, ci.mesh->vbh);
            bgfx::setIndexBuffer(ci.mesh->ibh);

            bool last = (j + 1 == group.indices.size());
            bgfx::submit(view, program_, 0, last ? BGFX_DISCARD_ALL : kGroupDiscard);
        }
    }
}

} // namespace recurse
