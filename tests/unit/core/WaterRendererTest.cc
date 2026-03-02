#include "recurse/render/WaterRenderer.hh"
#include "fixtures/BgfxNoopFixture.hh"

#include <gtest/gtest.h>

using namespace fabric;
using namespace recurse;
using fabric::test::BgfxNoopFixture;

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

TEST_F(BgfxNoopFixture, WaterRendererInitAndShutdownUnderNoop) {
    WaterRenderer renderer;
    // Trigger lazy initProgram() through render() with a valid mesh.
    // Noop renderer accepts all shader creates, so init succeeds.
    WaterChunkMesh mesh;
    mesh.valid = true;
    mesh.indexCount = 3;
    mesh.vbh = BGFX_INVALID_HANDLE;
    mesh.ibh = BGFX_INVALID_HANDLE;

    renderer.render(0, mesh, 0, 0, 0);
    EXPECT_TRUE(renderer.isValid());
    renderer.shutdown();
    EXPECT_FALSE(renderer.isValid());
}

TEST_F(BgfxNoopFixture, WaterRendererRenderWithValidMeshNoOp) {
    WaterRenderer renderer;
    // Render with valid-looking mesh but invalid buffer handles.
    // Lazy init triggers initProgram() which succeeds under noop.
    WaterChunkMesh mesh;
    mesh.valid = true;
    mesh.indexCount = 100;
    mesh.vbh = BGFX_INVALID_HANDLE;
    mesh.ibh = BGFX_INVALID_HANDLE;

    renderer.render(0, mesh, 0, 0, 0);
    EXPECT_TRUE(renderer.isValid());
    renderer.shutdown();
}
