#include "fabric/core/RenderCaps.hh"

#include <gtest/gtest.h>

// Pull in BGFX_CAPS_* defines for synthetic test data
#include <bgfx/bgfx.h>

using namespace fabric;

TEST(RenderCapsTest, DefaultState) {
    RenderCaps caps;
    EXPECT_EQ(caps.tier, RenderTier::Tier0);
    EXPECT_FALSE(caps.drawIndirect);
    EXPECT_FALSE(caps.compute);
    EXPECT_FALSE(caps.instancing);
    EXPECT_FALSE(caps.index32);
    EXPECT_FALSE(caps.mrt);
    EXPECT_FALSE(caps.imageRW);
    EXPECT_FALSE(caps.texture2DArray);
    EXPECT_FALSE(caps.texture3D);
}

TEST(RenderCapsTest, Tier0_Baseline) {
    // No flags set: baseline GPU (OpenGL ES 2 level)
    uint64_t flags = 0;
    auto tier = RenderCaps::classifyTier(flags, 1);
    EXPECT_EQ(tier, RenderTier::Tier0);
}

TEST(RenderCapsTest, Tier0_InstancingWithoutMRT) {
    // Instancing alone is not enough for Tier1
    uint64_t flags = BGFX_CAPS_INSTANCING;
    auto tier = RenderCaps::classifyTier(flags, 1);
    EXPECT_EQ(tier, RenderTier::Tier0);
}

TEST(RenderCapsTest, Tier0_MRTWithoutInstancing) {
    // MRT alone is not enough for Tier1
    uint64_t flags = 0;
    auto tier = RenderCaps::classifyTier(flags, 4);
    EXPECT_EQ(tier, RenderTier::Tier0);
}

TEST(RenderCapsTest, Tier1_InstancingAndMRT) {
    // Instancing + MRT (>1 FB attachment) = Tier1
    uint64_t flags = BGFX_CAPS_INSTANCING | BGFX_CAPS_INDEX32;
    auto tier = RenderCaps::classifyTier(flags, 4);
    EXPECT_EQ(tier, RenderTier::Tier1);
}

TEST(RenderCapsTest, Tier2_ComputeAndDrawIndirect) {
    // Compute + draw indirect = Tier2 (regardless of instancing/MRT)
    uint64_t flags = BGFX_CAPS_COMPUTE | BGFX_CAPS_DRAW_INDIRECT;
    auto tier = RenderCaps::classifyTier(flags, 1);
    EXPECT_EQ(tier, RenderTier::Tier2);
}

TEST(RenderCapsTest, Tier2_FullFeatureSet) {
    // All flags: still Tier2 (highest)
    uint64_t flags = BGFX_CAPS_COMPUTE | BGFX_CAPS_DRAW_INDIRECT | BGFX_CAPS_DRAW_INDIRECT_COUNT |
                     BGFX_CAPS_INSTANCING | BGFX_CAPS_INDEX32 | BGFX_CAPS_IMAGE_RW | BGFX_CAPS_TEXTURE_2D_ARRAY |
                     BGFX_CAPS_TEXTURE_3D;
    auto tier = RenderCaps::classifyTier(flags, 8);
    EXPECT_EQ(tier, RenderTier::Tier2);
}

TEST(RenderCapsTest, Tier2_ComputeWithoutIndirectCount) {
    // Compute + draw indirect (no indirect count) is still Tier2
    uint64_t flags = BGFX_CAPS_COMPUTE | BGFX_CAPS_DRAW_INDIRECT | BGFX_CAPS_INSTANCING;
    auto tier = RenderCaps::classifyTier(flags, 4);
    EXPECT_EQ(tier, RenderTier::Tier2);
}

TEST(RenderCapsTest, Tier1_ComputeAloneNotSufficient) {
    // Compute without draw indirect + instancing + MRT = Tier1
    uint64_t flags = BGFX_CAPS_COMPUTE | BGFX_CAPS_INSTANCING;
    auto tier = RenderCaps::classifyTier(flags, 4);
    EXPECT_EQ(tier, RenderTier::Tier1);
}

TEST(RenderCapsTest, InitFromBgfxRequiresRuntime) {
    GTEST_SKIP() << "Requires live bgfx runtime context to validate initFromBgfx().";
}
