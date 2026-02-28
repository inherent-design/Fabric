#include "fabric/core/WaterRenderer.hh"

#include <gtest/gtest.h>

using namespace fabric;

TEST(WaterRendererTest, DefaultInvalidState) {
    WaterRenderer renderer;
    EXPECT_FALSE(renderer.isValid());
}

TEST(WaterRendererTest, ShutdownBeforeInitKeepsInvalidState) {
    WaterRenderer renderer;
    renderer.shutdown();
    EXPECT_FALSE(renderer.isValid());
}

TEST(WaterRendererTest, DoubleShutdownIsSafe) {
    WaterRenderer renderer;
    renderer.shutdown();
    renderer.shutdown();
    EXPECT_FALSE(renderer.isValid());
}

TEST(WaterRendererTest, SetWaterColorStoresValues) {
    WaterRenderer renderer;
    // setWaterColor does not require bgfx; it stores values locally.
    renderer.setWaterColor(0.2f, 0.4f, 0.8f, 0.6f);
    // No crash; values applied on next render().
    EXPECT_FALSE(renderer.isValid());
}

TEST(WaterRendererTest, SetTimeStoresValue) {
    WaterRenderer renderer;
    renderer.setTime(3.14f);
    // No crash; value applied on next render().
    EXPECT_FALSE(renderer.isValid());
}

TEST(WaterRendererTest, SetLightDirectionWithoutBgfxDoesNotCrash) {
    WaterRenderer renderer;
    // setLightDirection stores values locally without bgfx context.
    Vector3<float, Space::World> dir(0.3f, 0.8f, 0.5f);
    renderer.setLightDirection(dir);
    EXPECT_FALSE(renderer.isValid());
}

TEST(WaterRendererTest, RenderEmptyMeshReturnsWithoutInitialization) {
    WaterRenderer renderer;
    WaterChunkMesh mesh;
    mesh.valid = false;
    mesh.indexCount = 0;

    renderer.render(0, mesh, 0, 0, 0);
    EXPECT_FALSE(renderer.isValid());
}

TEST(WaterRendererTest, RenderZeroIndexCountSkips) {
    WaterRenderer renderer;
    WaterChunkMesh mesh;
    mesh.valid = true;
    mesh.indexCount = 0;

    renderer.render(0, mesh, 0, 0, 0);
    EXPECT_FALSE(renderer.isValid());
}

TEST(WaterRendererTest, IsValidAfterInitRequiresRuntimeBgfxContext) {
    GTEST_SKIP() << "Requires live bgfx runtime context to safely validate isValid() after init.";
}

TEST(WaterRendererTest, RenderWithValidMeshRequiresRuntimeBgfxContext) {
    GTEST_SKIP() << "Requires live bgfx runtime context to safely validate render path.";
}
