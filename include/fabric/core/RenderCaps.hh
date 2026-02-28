#pragma once

#include <cstdint>
#include <string_view>

namespace fabric {

// Feature tier derived from GPU capability flags.
// Higher tiers are strict supersets of lower ones.
//
//   Tier0 - Baseline: vertex rendering, no instancing or compute.
//           Targets OpenGL 2.1 / ES 2.0.
//
//   Tier1 - Instancing + MRT + 32-bit indices.
//           Targets D3D11, OpenGL 3.3+, Metal (partial).
//
//   Tier2 - Compute shaders + draw indirect.
//           Targets Metal, Vulkan, D3D12.
enum class RenderTier : std::uint8_t {
    Tier0 = 0, // baseline
    Tier1 = 1, // instancing + MRT
    Tier2 = 2  // compute + indirect
};

std::string_view renderTierToString(RenderTier tier);

// Runtime GPU capability snapshot.
// Constructed once after bgfx::init() from bgfx::getCaps(), or from a
// synthetic bitmask for unit testing.
class RenderCaps {
  public:
    // Construct from live bgfx context (calls bgfx::getCaps()).
    // Must be called after bgfx::init().
    static RenderCaps fromDevice();

    // Construct from synthetic values (unit testing, offline analysis).
    static RenderCaps fromBitmask(uint64_t supported, uint32_t maxTextureSize, int rendererType);

    // Individual capability queries
    bool supportsCompute() const;
    bool supportsDrawIndirect() const;
    bool supportsDrawIndirectCount() const;
    bool supportsInstancing() const;
    bool supportsIndex32() const;
    bool supportsBlendIndependent() const;
    bool supportsImageRW() const;
    bool supportsTexture2DArray() const;
    bool supportsTexture3D() const;
    bool supportsMSAA() const;

    // Aggregate queries
    bool supportsMRT() const;

    // Limits
    uint32_t maxTextureSize() const;
    uint32_t maxFBAttachments() const;

    // Renderer identification
    int rendererType() const;
    std::string_view rendererName() const;

    // Derived tier
    RenderTier tier() const;

    // Raw bitmask (for logging / serialization)
    uint64_t supportedFlags() const;

  private:
    RenderCaps() = default;

    static RenderTier classifyTier(uint64_t supported);

    uint64_t supported_ = 0;
    uint32_t maxTextureSize_ = 0;
    uint32_t maxFBAttachments_ = 0;
    int rendererType_ = 0; // bgfx::RendererType::Enum stored as int
    RenderTier tier_ = RenderTier::Tier0;
};

} // namespace fabric
