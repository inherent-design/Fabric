#include "recurse/render/VoxelRenderer.hh"

#include "fabric/log/Log.hh"
#include "fabric/render/Rendering.hh"
#include "fabric/utils/Profiler.hh"

#include <algorithm>
#include <bx/math.h>
#include <vector>

#include "fabric/render/ShaderProgram.hh"
#include "fabric/render/SpvOnly.hh"

// Compiled SPIR-V shader bytecode generated at build time from .sc sources.
#include "spv/fs_smooth.sc.bin.h"
#include "spv/vs_smooth.sc.bin.h"

static const bgfx::EmbeddedShader s_voxelShaders[] = {BGFX_EMBEDDED_SHADER(vs_smooth), BGFX_EMBEDDED_SHADER(fs_smooth),
                                                      BGFX_EMBEDDED_SHADER_END()};

using namespace fabric;

namespace recurse {

VoxelRenderer::VoxelRenderer() = default;

VoxelRenderer::~VoxelRenderer() {
    shutdown();
}

void VoxelRenderer::shutdown() {
    indirectBuffer_.reset();
    uniformOceanParams_.reset();
    uniformRimParams_.reset();
    uniformShadowColor_.reset();
    uniformLitColor_.reset();
    uniformViewPos_.reset();
    uniformLightDir_.reset();
    uniformPalette_.reset();
    program_.reset();
    initialized_ = false;
    mdiSupported_ = false;
}

void VoxelRenderer::initProgram() {
    program_.reset(fabric::render::createProgramFromEmbedded(s_voxelShaders, "vs_smooth", "fs_smooth"));

    uniformPalette_.reset(bgfx::createUniform("u_palette", bgfx::UniformType::Vec4, 128));
    uniformLightDir_.reset(bgfx::createUniform("u_lightDir", bgfx::UniformType::Vec4));
    uniformViewPos_.reset(bgfx::createUniform("u_viewPos", bgfx::UniformType::Vec4));
    uniformLitColor_.reset(bgfx::createUniform("u_litColor", bgfx::UniformType::Vec4));
    uniformShadowColor_.reset(bgfx::createUniform("u_shadowColor", bgfx::UniformType::Vec4));
    uniformRimParams_.reset(bgfx::createUniform("u_rimParams", bgfx::UniformType::Vec4));
    uniformOceanParams_.reset(bgfx::createUniform("u_oceanParams", bgfx::UniformType::Vec4));

    if (!program_.isValid() || !uniformPalette_.isValid() || !uniformLightDir_.isValid() ||
        !uniformViewPos_.isValid() || !uniformLitColor_.isValid() || !uniformShadowColor_.isValid() ||
        !uniformRimParams_.isValid() || !uniformOceanParams_.isValid()) {
        FABRIC_LOG_RENDER_ERROR("VoxelRenderer shader/uniform init failed for renderer {}",
                                bgfx::getRendererName(bgfx::getRendererType()));
        shutdown();
        return;
    }

    constexpr float K_LIT_COLOR[4] = {0.85f, 0.85f, 0.85f, 1.0f};    // Neutral white-gray (was warm gold)
    constexpr float K_SHADOW_COLOR[4] = {0.35f, 0.35f, 0.40f, 1.0f}; // Neutral cool-gray (was purple-gray)
    constexpr float K_RIM_PARAMS[4] = {3.0f, 0.15f, 0.0f, 0.0f};
    constexpr float K_OCEAN_PARAMS[4] = {16.0f, 0.2f, 0.0f, 0.0f};

    std::copy(std::begin(K_LIT_COLOR), std::end(K_LIT_COLOR), litColor_);
    std::copy(std::begin(K_SHADOW_COLOR), std::end(K_SHADOW_COLOR), shadowColor_);
    std::copy(std::begin(K_RIM_PARAMS), std::end(K_RIM_PARAMS), rimParams_);
    std::copy(std::begin(K_OCEAN_PARAMS), std::end(K_OCEAN_PARAMS), oceanParams_);

    initialized_ = true;

    const auto& caps = renderCaps();
    FABRIC_LOG_RENDER_INFO("VoxelRenderer initialized: backend={}, tier={}", caps.rendererName,
                           static_cast<int>(caps.tier));
    if (!caps.index32) {
        FABRIC_LOG_RENDER_WARN("VoxelRenderer: 32-bit indices unavailable; chunk meshes limited to 65535 vertices");
    }

    mdiSupported_ = caps.drawIndirect;
    if (mdiSupported_) {
        indirectBuffer_.reset(bgfx::createIndirectBuffer(K_MAX_INDIRECT_DRAWS));
        if (!indirectBuffer_.isValid()) {
            FABRIC_LOG_RENDER_WARN("VoxelRenderer: indirect buffer allocation failed, MDI disabled");
            mdiSupported_ = false;
        } else {
            FABRIC_LOG_RENDER_INFO("VoxelRenderer: MDI enabled ({} max draws)", K_MAX_INDIRECT_DRAWS);
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

    if (!isValid() || !mesh.vbh.isValid() || !mesh.ibh.isValid()) {
        return;
    }

    // Camera-relative transform supplied by caller.
    float mtx[16];
    bx::mtxIdentity(mtx);
    mtx[12] = offsetX;
    mtx[13] = offsetY;
    mtx[14] = offsetZ;
    bgfx::setTransform(mtx);

    // Upload palette colors (up to 128 entries; bgfx shaderc stores array
    // count as uint8_t so 256 overflows to 0, breaking uniform upload).
    if (!mesh.palette.empty()) {
        auto count = static_cast<uint16_t>(std::min(mesh.palette.size(), static_cast<size_t>(128)));
        bgfx::setUniform(uniformPalette_.get(), mesh.palette.data(), count);
    }

    bgfx::setVertexBuffer(0, mesh.vbh.get());
    bgfx::setIndexBuffer(mesh.ibh.get());

    bgfx::setState(renderState());
    bgfx::setUniform(uniformLightDir_.get(), lightDir_);
    bgfx::setUniform(uniformViewPos_.get(), viewPos_);
    bgfx::setUniform(uniformLitColor_.get(), litColor_);
    bgfx::setUniform(uniformShadowColor_.get(), shadowColor_);
    bgfx::setUniform(uniformRimParams_.get(), rimParams_);
    bgfx::setUniform(uniformOceanParams_.get(), oceanParams_);
    bgfx::submit(view, program_.get());
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

void VoxelRenderer::setWireframeEnabled(bool enabled) {
    wireframeEnabled_ = enabled;
}

bool VoxelRenderer::isWireframeEnabled() const {
    return wireframeEnabled_;
}

bool VoxelRenderer::isValid() const {
    return program_.isValid();
}

bool VoxelRenderer::mdiSupported() const {
    return mdiSupported_;
}

uint64_t VoxelRenderer::renderState() const {
    uint64_t state =
        BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_MSAA;
    state |= BGFX_STATE_CULL_CCW;
    return state;
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
        if (!ci.mesh->vbh.isValid() || !ci.mesh->ibh.isValid()) {
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

    const uint64_t state = renderState();

    // Discard per-chunk resources; preserve uniforms + render state
    constexpr uint8_t K_GROUP_DISCARD =
        BGFX_DISCARD_INDEX_BUFFER | BGFX_DISCARD_INSTANCE_DATA | BGFX_DISCARD_TRANSFORM | BGFX_DISCARD_VERTEX_STREAMS;

    for (const auto& group : groups) {
        if (!group.palette->empty()) {
            auto cnt = static_cast<uint16_t>(std::min(group.palette->size(), static_cast<size_t>(128)));
            bgfx::setUniform(uniformPalette_.get(), group.palette->data(), cnt);
        }
        bgfx::setUniform(uniformLightDir_.get(), lightDir_);
        bgfx::setUniform(uniformViewPos_.get(), viewPos_);
        bgfx::setUniform(uniformLitColor_.get(), litColor_);
        bgfx::setUniform(uniformShadowColor_.get(), shadowColor_);
        bgfx::setUniform(uniformRimParams_.get(), rimParams_);
        bgfx::setUniform(uniformOceanParams_.get(), oceanParams_);
        bgfx::setState(state);

        for (size_t j = 0; j < group.indices.size(); ++j) {
            const auto& ci = chunks[group.indices[j]];

            float mtx[16];
            bx::mtxIdentity(mtx);
            mtx[12] = ci.offsetX;
            mtx[13] = ci.offsetY;
            mtx[14] = ci.offsetZ;
            bgfx::setTransform(mtx);

            bgfx::setVertexBuffer(0, ci.mesh->vbh.get());
            bgfx::setIndexBuffer(ci.mesh->ibh.get());

            bool last = (j + 1 == group.indices.size());
            bgfx::submit(view, program_.get(), 0, last ? BGFX_DISCARD_ALL : K_GROUP_DISCARD);
        }
    }
}

} // namespace recurse
