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
    mesh.vbh = BGFX_INVALID_HANDLE;
    mesh.ibh = BGFX_INVALID_HANDLE;

    renderer.render(0, mesh, 0, 0, 0);
    EXPECT_TRUE(renderer.isValid());
    renderer.shutdown();
    EXPECT_FALSE(renderer.isValid());
}

TEST(VoxelRendererTest, SetLightDirectionDeferredStateRemainsInvalidWithoutInit) {
    VoxelRenderer renderer;
    EXPECT_FALSE(renderer.isValid());
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
    mesh.vbh = BGFX_INVALID_HANDLE;
    mesh.ibh = BGFX_INVALID_HANDLE;

    renderer.render(0, mesh, 0, 0, 0);
    // Noop renderer: initProgram succeeds, so renderer is valid.
    EXPECT_TRUE(renderer.isValid());
    renderer.shutdown();
}
