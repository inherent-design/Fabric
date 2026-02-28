#include "fabric/core/PostProcess.hh"

#include <gtest/gtest.h>

using namespace fabric;

TEST(PostProcessTest, DefaultInvalidState) {
    PostProcess pp;
    EXPECT_FALSE(pp.isValid());
}

TEST(PostProcessTest, ShutdownBeforeInitIsNoOp) {
    PostProcess pp;
    pp.shutdown();
    EXPECT_FALSE(pp.isValid());
}

TEST(PostProcessTest, DoubleShutdownIsNoOp) {
    PostProcess pp;
    pp.shutdown();
    pp.shutdown();
    EXPECT_FALSE(pp.isValid());
}

TEST(PostProcessTest, DefaultThreshold) {
    PostProcess pp;
    EXPECT_FLOAT_EQ(pp.threshold(), 1.0f);
}

TEST(PostProcessTest, SetThreshold) {
    PostProcess pp;
    pp.setThreshold(0.8f);
    EXPECT_FLOAT_EQ(pp.threshold(), 0.8f);
}

TEST(PostProcessTest, DefaultIntensity) {
    PostProcess pp;
    EXPECT_FLOAT_EQ(pp.intensity(), 0.5f);
}

TEST(PostProcessTest, SetIntensity) {
    PostProcess pp;
    pp.setIntensity(1.5f);
    EXPECT_FLOAT_EQ(pp.intensity(), 1.5f);
}

TEST(PostProcessTest, DefaultExposure) {
    PostProcess pp;
    EXPECT_FLOAT_EQ(pp.exposure(), 1.0f);
}

TEST(PostProcessTest, SetExposure) {
    PostProcess pp;
    pp.setExposure(2.0f);
    EXPECT_FLOAT_EQ(pp.exposure(), 2.0f);
}

TEST(PostProcessTest, ParametersBeforeInit) {
    PostProcess pp;
    pp.setThreshold(0.5f);
    pp.setIntensity(2.0f);
    pp.setExposure(0.8f);
    EXPECT_FLOAT_EQ(pp.threshold(), 0.5f);
    EXPECT_FLOAT_EQ(pp.intensity(), 2.0f);
    EXPECT_FLOAT_EQ(pp.exposure(), 0.8f);
    EXPECT_FALSE(pp.isValid());
}

TEST(PostProcessTest, RenderWithoutInitIsNoOp) {
    PostProcess pp;
    // render() is safe before init() -- returns early when not valid
    pp.render(200);
    EXPECT_FALSE(pp.isValid());
}

TEST(PostProcessTest, ResizeBeforeInitStoresDimensions) {
    PostProcess pp;
    pp.resize(1920, 1080);
    // Resize before init only stores dimensions, no framebuffer created
    EXPECT_FALSE(pp.isValid());
}

TEST(PostProcessTest, ResizeWithZeroDimensionsIgnored) {
    PostProcess pp;
    pp.resize(0, 0);
    EXPECT_FALSE(pp.isValid());
}

TEST(PostProcessTest, HdrFramebufferInvalidBeforeInit) {
    PostProcess pp;
    auto fb = pp.hdrFramebuffer();
    EXPECT_FALSE(bgfx::isValid(fb));
}

TEST(PostProcessTest, InitRequiresRuntimeBgfxContext) {
    GTEST_SKIP() << "Requires live bgfx runtime context to safely validate init behavior.";
}

TEST(PostProcessTest, InitWithZeroDimensionsDoesNotCrash) {
    PostProcess pp;
    pp.init(0, 0);
    EXPECT_FALSE(pp.isValid());
}
