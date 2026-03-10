#pragma once

#include <cstdint>

namespace fabric {

// Feature tier classification based on GPU capabilities.
// Tier0: baseline (OpenGL ES 2 level)
// Tier1: instancing + MRT
// Tier2: compute + draw indirect
enum class RenderTier : uint8_t {
    Tier0 = 0, // Baseline: vertex buffers, basic textures
    Tier1 = 1, // Instancing, MRT, index32
    Tier2 = 2  // Compute shaders, draw indirect
};

// Runtime capability detection layer wrapping bgfx::getCaps().
// Query once after bgfx::init(), then use typed accessors to gate
// optional rendering features behind capability flags.
struct RenderCaps {
    // Individual capability flags
    bool drawIndirect = false;
    bool drawIndirectCount = false;
    bool compute = false;
    bool instancing = false;
    bool index32 = false;
    bool mrt = false; // Multiple render targets (maxFBAttachments > 1)
    bool imageRW = false;
    bool texture2DArray = false;
    bool texture3D = false;

    // Renderer info
    uint16_t maxFBAttachments = 1;
    uint32_t maxDrawCalls = 0;
    uint32_t maxTextureSize = 0;
    const char* rendererName = "Unknown";

    // Computed tier
    RenderTier tier = RenderTier::Tier0;

    // Initialize from live bgfx::getCaps(). Call after bgfx::init().
    void initFromBgfx();

    // Classify tier from a raw supported flags bitmask and attachment count.
    // Useful for unit testing with synthetic values.
    static RenderTier classifyTier(uint64_t supportedFlags, uint16_t fbAttachments);
};

} // namespace fabric
