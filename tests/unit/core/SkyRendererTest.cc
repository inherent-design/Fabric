#include "fabric/core/SkyRenderer.hh"

#include <gtest/gtest.h>

using namespace fabric;

TEST(SkyRendererTest, DefaultInvalidState) {
    SkyRenderer renderer;
    EXPECT_FALSE(renderer.isValid());
}

TEST(SkyRendererTest, ShutdownBeforeInitKeepsInvalidState) {
    SkyRenderer renderer;
    renderer.shutdown();
    EXPECT_FALSE(renderer.isValid());
}

TEST(SkyRendererTest, DoubleShutdownIsNoOp) {
    SkyRenderer renderer;
    renderer.shutdown();
    renderer.shutdown();
    EXPECT_FALSE(renderer.isValid());
}

TEST(SkyRendererTest, DefaultSunDirection) {
    SkyRenderer renderer;
    auto dir = renderer.sunDirection();
    // Default sun direction is (0, 0.7071, 0.7071) -- 45 degrees elevation
    EXPECT_NEAR(dir.x, 0.0f, 1e-4f);
    EXPECT_NEAR(dir.y, 0.7071f, 1e-3f);
    EXPECT_NEAR(dir.z, 0.7071f, 1e-3f);
}

TEST(SkyRendererTest, SetSunDirectionUpdatesState) {
    SkyRenderer renderer;
    Vector3<float, Space::World> newDir(0.0f, 1.0f, 0.0f);
    renderer.setSunDirection(newDir);

    auto dir = renderer.sunDirection();
    EXPECT_FLOAT_EQ(dir.x, 0.0f);
    EXPECT_FLOAT_EQ(dir.y, 1.0f);
    EXPECT_FLOAT_EQ(dir.z, 0.0f);
}

TEST(SkyRendererTest, RenderWithoutInitIsNoOp) {
    SkyRenderer renderer;
    // render() is safe before init() -- returns early when program is invalid
    renderer.render(0);
    EXPECT_FALSE(renderer.isValid());
}

TEST(SkyRendererTest, InitRequiresRuntimeBgfxContext) {
    GTEST_SKIP() << "Requires live bgfx runtime context to safely validate init behavior.";
}

TEST(SkyRendererTest, SetSunDirectionBeforeInitDoesNotCrash) {
    SkyRenderer renderer;
    Vector3<float, Space::World> dir(0.577f, 0.577f, 0.577f);
    renderer.setSunDirection(dir);
    EXPECT_FALSE(renderer.isValid());
    auto result = renderer.sunDirection();
    EXPECT_NEAR(result.x, 0.577f, 1e-3f);
}
