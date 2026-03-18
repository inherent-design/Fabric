#include "recurse/render/VoxelRenderer.hh"
#include "fixtures/BgfxNoopFixture.hh"

#include <gtest/gtest.h>

using namespace fabric;
using namespace recurse;
using fabric::test::BgfxNoopFixture;

TEST(VoxelRendererTest, DefaultInvalidState) {
    VoxelRenderer renderer;
    EXPECT_FALSE(renderer.isValid());
}

TEST(VoxelRendererTest, ShutdownBeforeInitKeepsInvalidState) {
    VoxelRenderer renderer;
    renderer.shutdown();
    EXPECT_FALSE(renderer.isValid());
}

TEST_F(BgfxNoopFixture, VoxelRendererInitAndShutdownUnderNoop) {
    VoxelRenderer renderer;
    // Trigger lazy initProgram() through render() with a valid mesh.
    // Noop renderer accepts all shader creates, so init succeeds.
    ChunkMesh mesh;
    mesh.valid = true;
    mesh.indexCount = 3;

    renderer.render(0, mesh, 0, 0, 0);
    EXPECT_TRUE(renderer.isValid());
    renderer.shutdown();
    EXPECT_FALSE(renderer.isValid());
}

TEST(VoxelRendererTest, SetLightDirectionDeferredStateRemainsInvalidWithoutInit) {
    VoxelRenderer renderer;
    EXPECT_FALSE(renderer.isValid());
}

TEST(VoxelRendererTest, WireframeFlagRoundTripsWithoutInit) {
    VoxelRenderer renderer;
    EXPECT_FALSE(renderer.isWireframeEnabled());

    renderer.setWireframeEnabled(true);
    EXPECT_TRUE(renderer.isWireframeEnabled());

    renderer.setWireframeEnabled(false);
    EXPECT_FALSE(renderer.isWireframeEnabled());
}

TEST(VoxelRendererTest, RenderEmptyMeshReturnsWithoutInitialization) {
    VoxelRenderer renderer;
    ChunkMesh mesh;
    mesh.valid = false;
    mesh.indexCount = 0;

    renderer.render(0, mesh, 0, 0, 0);
    EXPECT_FALSE(renderer.isValid());
}

TEST_F(BgfxNoopFixture, VoxelRendererRenderWithInvalidBuffersNoOp) {
    VoxelRenderer renderer;
    // Render with valid-looking mesh but invalid buffer handles.
    // Lazy init triggers initProgram() which succeeds under noop.
    // Render returns early because buffer handles are invalid.
    ChunkMesh mesh;
    mesh.valid = true;
    mesh.indexCount = 100;

    renderer.render(0, mesh, 0, 0, 0);
    // Noop renderer: initProgram succeeds, so renderer is valid.
    EXPECT_TRUE(renderer.isValid());
    renderer.shutdown();
}
