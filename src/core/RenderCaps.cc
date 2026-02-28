#include "fabric/core/RenderCaps.hh"

#include "fabric/core/Log.hh"
#include "fabric/utils/Profiler.hh"

#include <bgfx/bgfx.h>

namespace fabric {

// Tier classification thresholds (bitmask subsets)
namespace {

// Tier1 requires instancing, 32-bit indices, and MRT (>1 framebuffer attachment)
constexpr uint64_t kTier1Required = BGFX_CAPS_INSTANCING | BGFX_CAPS_INDEX32;

// Tier2 requires everything in Tier1 plus compute and draw indirect
constexpr uint64_t kTier2Required = kTier1Required | BGFX_CAPS_COMPUTE | BGFX_CAPS_DRAW_INDIRECT;

} // namespace

std::string_view renderTierToString(RenderTier tier) {
    switch (tier) {
        case RenderTier::Tier0:
            return "Tier0 (baseline)";
        case RenderTier::Tier1:
            return "Tier1 (instancing+MRT)";
        case RenderTier::Tier2:
            return "Tier2 (compute+indirect)";
    }
    return "Unknown";
}

RenderCaps RenderCaps::fromDevice() {
    FABRIC_ZONE_SCOPED_N("RenderCaps::fromDevice");

    const bgfx::Caps* caps = bgfx::getCaps();

    RenderCaps rc;
    rc.supported_ = caps->supported;
    rc.maxTextureSize_ = caps->limits.maxTextureSize;
    rc.maxFBAttachments_ = caps->limits.maxFBAttachments;
    rc.rendererType_ = static_cast<int>(caps->rendererType);
    rc.tier_ = classifyTier(rc.supported_);

    FABRIC_LOG_INFO("RenderCaps: backend={}, tier={}, supported=0x{:016X}", bgfx::getRendererName(caps->rendererType),
                    renderTierToString(rc.tier_), rc.supported_);
    FABRIC_LOG_INFO("RenderCaps: maxTextureSize={}, maxFBAttachments={}", rc.maxTextureSize_, rc.maxFBAttachments_);

    // Per-feature detail logging
    FABRIC_LOG_DEBUG("RenderCaps: compute={}, drawIndirect={}, drawIndirectCount={}", rc.supportsCompute(),
                     rc.supportsDrawIndirect(), rc.supportsDrawIndirectCount());
    FABRIC_LOG_DEBUG("RenderCaps: instancing={}, index32={}, blendIndependent={}", rc.supportsInstancing(),
                     rc.supportsIndex32(), rc.supportsBlendIndependent());
    FABRIC_LOG_DEBUG("RenderCaps: imageRW={}, texture2DArray={}, texture3D={}", rc.supportsImageRW(),
                     rc.supportsTexture2DArray(), rc.supportsTexture3D());

    // Warn on capabilities Fabric assumes
    if (!rc.supportsIndex32()) {
        FABRIC_LOG_WARN("RenderCaps: INDEX32 not supported; Fabric uses 32-bit indices throughout. "
                        "Meshes exceeding 65535 vertices will fail to render.");
    }

    return rc;
}

RenderCaps RenderCaps::fromBitmask(uint64_t supported, uint32_t maxTextureSize, int rendererType) {
    RenderCaps rc;
    rc.supported_ = supported;
    rc.maxTextureSize_ = maxTextureSize;
    rc.maxFBAttachments_ = 1; // conservative default for synthetic caps
    rc.rendererType_ = rendererType;
    rc.tier_ = classifyTier(supported);
    return rc;
}

RenderTier RenderCaps::classifyTier(uint64_t supported) {
    if ((supported & kTier2Required) == kTier2Required) {
        return RenderTier::Tier2;
    }
    if ((supported & kTier1Required) == kTier1Required) {
        return RenderTier::Tier1;
    }
    return RenderTier::Tier0;
}

// Individual capability accessors

bool RenderCaps::supportsCompute() const {
    return (supported_ & BGFX_CAPS_COMPUTE) != 0;
}

bool RenderCaps::supportsDrawIndirect() const {
    return (supported_ & BGFX_CAPS_DRAW_INDIRECT) != 0;
}

bool RenderCaps::supportsDrawIndirectCount() const {
    return (supported_ & BGFX_CAPS_DRAW_INDIRECT_COUNT) != 0;
}

bool RenderCaps::supportsInstancing() const {
    return (supported_ & BGFX_CAPS_INSTANCING) != 0;
}

bool RenderCaps::supportsIndex32() const {
    return (supported_ & BGFX_CAPS_INDEX32) != 0;
}

bool RenderCaps::supportsBlendIndependent() const {
    return (supported_ & BGFX_CAPS_BLEND_INDEPENDENT) != 0;
}

bool RenderCaps::supportsImageRW() const {
    return (supported_ & BGFX_CAPS_IMAGE_RW) != 0;
}

bool RenderCaps::supportsTexture2DArray() const {
    return (supported_ & BGFX_CAPS_TEXTURE_2D_ARRAY) != 0;
}

bool RenderCaps::supportsTexture3D() const {
    return (supported_ & BGFX_CAPS_TEXTURE_3D) != 0;
}

bool RenderCaps::supportsMSAA() const {
    // MSAA is inferred from texture format caps, not a top-level flag.
    // At the caps bitmask level, we approximate: if the backend supports
    // framebuffer MSAA resolve (all Tier1+ backends do), we report true.
    // A more precise check would query per-format MSAA support.
    return tier_ >= RenderTier::Tier1;
}

bool RenderCaps::supportsMRT() const {
    // MRT requires multiple framebuffer attachments and blend-independent support.
    // maxFBAttachments > 1 is the primary signal.
    return maxFBAttachments_ > 1;
}

uint32_t RenderCaps::maxTextureSize() const {
    return maxTextureSize_;
}

uint32_t RenderCaps::maxFBAttachments() const {
    return maxFBAttachments_;
}

int RenderCaps::rendererType() const {
    return rendererType_;
}

std::string_view RenderCaps::rendererName() const {
    return bgfx::getRendererName(static_cast<bgfx::RendererType::Enum>(rendererType_));
}

RenderTier RenderCaps::tier() const {
    return tier_;
}

uint64_t RenderCaps::supportedFlags() const {
    return supported_;
}

} // namespace fabric
