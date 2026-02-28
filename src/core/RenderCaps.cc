#include "fabric/core/RenderCaps.hh"

#include "fabric/core/Log.hh"
#include "fabric/utils/Profiler.hh"

#include <bgfx/bgfx.h>

namespace fabric {

void RenderCaps::initFromBgfx() {
    FABRIC_ZONE_SCOPED_N("RenderCaps::initFromBgfx");

    const bgfx::Caps* caps = bgfx::getCaps();
    if (!caps) {
        FABRIC_LOG_ERROR("RenderCaps: bgfx::getCaps() returned null; bgfx not initialized?");
        return;
    }

    uint64_t s = caps->supported;
    drawIndirect = (s & BGFX_CAPS_DRAW_INDIRECT) != 0;
    drawIndirectCount = (s & BGFX_CAPS_DRAW_INDIRECT_COUNT) != 0;
    compute = (s & BGFX_CAPS_COMPUTE) != 0;
    instancing = (s & BGFX_CAPS_INSTANCING) != 0;
    index32 = (s & BGFX_CAPS_INDEX32) != 0;
    imageRW = (s & BGFX_CAPS_IMAGE_RW) != 0;
    texture2DArray = (s & BGFX_CAPS_TEXTURE_2D_ARRAY) != 0;
    texture3D = (s & BGFX_CAPS_TEXTURE_3D) != 0;

    maxFBAttachments = static_cast<uint16_t>(caps->limits.maxFBAttachments);
    maxDrawCalls = caps->limits.maxDrawCalls;
    maxTextureSize = caps->limits.maxTextureSize;
    mrt = maxFBAttachments > 1;

    rendererName = bgfx::getRendererName(caps->rendererType);
    tier = classifyTier(s, maxFBAttachments);

    if (!index32) {
        FABRIC_LOG_WARN("RenderCaps: BGFX_CAPS_INDEX32 not supported; large meshes will require splitting");
    }

    FABRIC_LOG_INFO("RenderCaps: backend={}, tier={}, instancing={}, compute={}, drawIndirect={}, mrt={}, index32={}",
                    rendererName, static_cast<int>(tier), instancing, compute, drawIndirect, mrt, index32);
}

RenderTier RenderCaps::classifyTier(uint64_t supportedFlags, uint16_t fbAttachments) {
    bool hasCompute = (supportedFlags & BGFX_CAPS_COMPUTE) != 0;
    bool hasDrawIndirect = (supportedFlags & BGFX_CAPS_DRAW_INDIRECT) != 0;
    bool hasInstancing = (supportedFlags & BGFX_CAPS_INSTANCING) != 0;
    bool hasMRT = fbAttachments > 1;

    if (hasCompute && hasDrawIndirect) {
        return RenderTier::Tier2;
    }
    if (hasInstancing && hasMRT) {
        return RenderTier::Tier1;
    }
    return RenderTier::Tier0;
}

} // namespace fabric
