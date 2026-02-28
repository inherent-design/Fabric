#include "fabric/core/RenderCaps.hh"

#include <bgfx/bgfx.h>
#include <gtest/gtest.h>

using namespace fabric;

// Synthetic bitmask constants matching bgfx defines.
// Duplicated here so tests remain readable without chasing macro values.
namespace {

constexpr uint64_t kCompute = BGFX_CAPS_COMPUTE;
constexpr uint64_t kDrawIndirect = BGFX_CAPS_DRAW_INDIRECT;
constexpr uint64_t kDrawIndirectCount = BGFX_CAPS_DRAW_INDIRECT_COUNT;
constexpr uint64_t kInstancing = BGFX_CAPS_INSTANCING;
constexpr uint64_t kIndex32 = BGFX_CAPS_INDEX32;
constexpr uint64_t kBlendIndependent = BGFX_CAPS_BLEND_INDEPENDENT;
constexpr uint64_t kImageRW = BGFX_CAPS_IMAGE_RW;
constexpr uint64_t kTexture2DArray = BGFX_CAPS_TEXTURE_2D_ARRAY;
constexpr uint64_t kTexture3D = BGFX_CAPS_TEXTURE_3D;

// Composite bitmasks simulating real backends
constexpr uint64_t kOpenGLES2Caps = 0; // baseline, no advanced caps
constexpr uint64_t kD3D11Caps = kInstancing | kIndex32 | kCompute | kBlendIndependent | kTexture2DArray | kTexture3D;
constexpr uint64_t kMetalCaps = kInstancing | kIndex32 | kCompute | kDrawIndirect | kDrawIndirectCount |
                                kBlendIndependent | kImageRW | kTexture2DArray | kTexture3D;

constexpr uint32_t kDefaultMaxTexSize = 8192;

} // namespace

// -- Tier classification tests --

TEST(RenderCapsTest, Tier0FromEmptyBitmask) {
    auto caps = RenderCaps::fromBitmask(0, kDefaultMaxTexSize, bgfx::RendererType::Noop);
    EXPECT_EQ(caps.tier(), RenderTier::Tier0);
}

TEST(RenderCapsTest, Tier0FromOpenGLES2) {
    auto caps =
        RenderCaps::fromBitmask(kOpenGLES2Caps, kDefaultMaxTexSize, static_cast<int>(bgfx::RendererType::OpenGLES));
    EXPECT_EQ(caps.tier(), RenderTier::Tier0);
    EXPECT_FALSE(caps.supportsCompute());
    EXPECT_FALSE(caps.supportsDrawIndirect());
    EXPECT_FALSE(caps.supportsInstancing());
    EXPECT_FALSE(caps.supportsIndex32());
}

TEST(RenderCapsTest, Tier1FromD3D11) {
    auto caps =
        RenderCaps::fromBitmask(kD3D11Caps, kDefaultMaxTexSize, static_cast<int>(bgfx::RendererType::Direct3D11));
    EXPECT_EQ(caps.tier(), RenderTier::Tier1);
    EXPECT_TRUE(caps.supportsInstancing());
    EXPECT_TRUE(caps.supportsIndex32());
    EXPECT_TRUE(caps.supportsCompute());
    EXPECT_FALSE(caps.supportsDrawIndirect());
}

TEST(RenderCapsTest, Tier2FromMetal) {
    auto caps = RenderCaps::fromBitmask(kMetalCaps, 16384, static_cast<int>(bgfx::RendererType::Metal));
    EXPECT_EQ(caps.tier(), RenderTier::Tier2);
    EXPECT_TRUE(caps.supportsCompute());
    EXPECT_TRUE(caps.supportsDrawIndirect());
    EXPECT_TRUE(caps.supportsDrawIndirectCount());
    EXPECT_TRUE(caps.supportsInstancing());
    EXPECT_TRUE(caps.supportsIndex32());
    EXPECT_TRUE(caps.supportsImageRW());
}

TEST(RenderCapsTest, Tier1NeedsInstancingAndIndex32) {
    // Instancing alone is not enough for Tier1 (missing INDEX32)
    auto capsInstOnly = RenderCaps::fromBitmask(kInstancing, kDefaultMaxTexSize, bgfx::RendererType::Noop);
    EXPECT_EQ(capsInstOnly.tier(), RenderTier::Tier0);

    // INDEX32 alone is not enough for Tier1 (missing instancing)
    auto capsIdx32Only = RenderCaps::fromBitmask(kIndex32, kDefaultMaxTexSize, bgfx::RendererType::Noop);
    EXPECT_EQ(capsIdx32Only.tier(), RenderTier::Tier0);

    // Both together reach Tier1
    auto capsBoth = RenderCaps::fromBitmask(kInstancing | kIndex32, kDefaultMaxTexSize, bgfx::RendererType::Noop);
    EXPECT_EQ(capsBoth.tier(), RenderTier::Tier1);
}

TEST(RenderCapsTest, Tier2NeedsComputeAndDrawIndirect) {
    // Tier1 caps + compute alone is not Tier2 (missing draw indirect)
    auto capsNoIndirect =
        RenderCaps::fromBitmask(kInstancing | kIndex32 | kCompute, kDefaultMaxTexSize, bgfx::RendererType::Noop);
    EXPECT_EQ(capsNoIndirect.tier(), RenderTier::Tier1);

    // Tier1 caps + draw indirect alone is not Tier2 (missing compute)
    auto capsNoCompute =
        RenderCaps::fromBitmask(kInstancing | kIndex32 | kDrawIndirect, kDefaultMaxTexSize, bgfx::RendererType::Noop);
    EXPECT_EQ(capsNoCompute.tier(), RenderTier::Tier1);

    // All four together reach Tier2
    auto capsFull = RenderCaps::fromBitmask(kInstancing | kIndex32 | kCompute | kDrawIndirect, kDefaultMaxTexSize,
                                            bgfx::RendererType::Noop);
    EXPECT_EQ(capsFull.tier(), RenderTier::Tier2);
}

// -- Individual flag accessor tests --

TEST(RenderCapsTest, IndividualFlagAccessors) {
    auto caps = RenderCaps::fromBitmask(kMetalCaps, kDefaultMaxTexSize, bgfx::RendererType::Noop);

    EXPECT_TRUE(caps.supportsCompute());
    EXPECT_TRUE(caps.supportsDrawIndirect());
    EXPECT_TRUE(caps.supportsDrawIndirectCount());
    EXPECT_TRUE(caps.supportsInstancing());
    EXPECT_TRUE(caps.supportsIndex32());
    EXPECT_TRUE(caps.supportsBlendIndependent());
    EXPECT_TRUE(caps.supportsImageRW());
    EXPECT_TRUE(caps.supportsTexture2DArray());
    EXPECT_TRUE(caps.supportsTexture3D());
}

TEST(RenderCapsTest, IndividualFlagAccessorsAllFalseOnEmpty) {
    auto caps = RenderCaps::fromBitmask(0, kDefaultMaxTexSize, bgfx::RendererType::Noop);

    EXPECT_FALSE(caps.supportsCompute());
    EXPECT_FALSE(caps.supportsDrawIndirect());
    EXPECT_FALSE(caps.supportsDrawIndirectCount());
    EXPECT_FALSE(caps.supportsInstancing());
    EXPECT_FALSE(caps.supportsIndex32());
    EXPECT_FALSE(caps.supportsBlendIndependent());
    EXPECT_FALSE(caps.supportsImageRW());
    EXPECT_FALSE(caps.supportsTexture2DArray());
    EXPECT_FALSE(caps.supportsTexture3D());
}

// -- Limits and renderer info --

TEST(RenderCapsTest, MaxTextureSize) {
    auto caps = RenderCaps::fromBitmask(0, 4096, bgfx::RendererType::Noop);
    EXPECT_EQ(caps.maxTextureSize(), 4096u);

    auto capsLarge = RenderCaps::fromBitmask(0, 16384, bgfx::RendererType::Noop);
    EXPECT_EQ(capsLarge.maxTextureSize(), 16384u);
}

TEST(RenderCapsTest, RendererTypePreserved) {
    auto capsMetal = RenderCaps::fromBitmask(0, kDefaultMaxTexSize, static_cast<int>(bgfx::RendererType::Metal));
    EXPECT_EQ(capsMetal.rendererType(), static_cast<int>(bgfx::RendererType::Metal));

    auto capsVulkan = RenderCaps::fromBitmask(0, kDefaultMaxTexSize, static_cast<int>(bgfx::RendererType::Vulkan));
    EXPECT_EQ(capsVulkan.rendererType(), static_cast<int>(bgfx::RendererType::Vulkan));
}

TEST(RenderCapsTest, SupportedFlagsRoundTrip) {
    auto caps = RenderCaps::fromBitmask(kMetalCaps, kDefaultMaxTexSize, bgfx::RendererType::Noop);
    EXPECT_EQ(caps.supportedFlags(), kMetalCaps);
}

// -- MSAA and MRT aggregate queries --

TEST(RenderCapsTest, MSAARequiresTier1) {
    auto capsTier0 = RenderCaps::fromBitmask(0, kDefaultMaxTexSize, bgfx::RendererType::Noop);
    EXPECT_FALSE(capsTier0.supportsMSAA());

    auto capsTier1 = RenderCaps::fromBitmask(kInstancing | kIndex32, kDefaultMaxTexSize, bgfx::RendererType::Noop);
    EXPECT_TRUE(capsTier1.supportsMSAA());
}

// -- Tier string conversion --

TEST(RenderCapsTest, TierToStringCoversAllValues) {
    EXPECT_EQ(renderTierToString(RenderTier::Tier0), "Tier0 (baseline)");
    EXPECT_EQ(renderTierToString(RenderTier::Tier1), "Tier1 (instancing+MRT)");
    EXPECT_EQ(renderTierToString(RenderTier::Tier2), "Tier2 (compute+indirect)");
}

// -- Renderer name via bgfx --

TEST(RenderCapsTest, RendererNameForNoop) {
    auto caps = RenderCaps::fromBitmask(0, kDefaultMaxTexSize, static_cast<int>(bgfx::RendererType::Noop));
    // bgfx returns "Noop" for the noop renderer
    EXPECT_FALSE(caps.rendererName().empty());
}
