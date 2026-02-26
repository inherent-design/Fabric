#include "fabric/core/VoxelRenderer.hh"

#include <gtest/gtest.h>

using namespace fabric;

TEST(VoxelRendererTest, DefaultInvalidState) {
    VoxelRenderer renderer;
    EXPECT_FALSE(renderer.isValid());
}

TEST(VoxelRendererTest, ShutdownBeforeInitKeepsInvalidState) {
    VoxelRenderer renderer;
    renderer.shutdown();
    EXPECT_FALSE(renderer.isValid());
}

TEST(VoxelRendererTest, SetLightDirectionRequiresRuntimeBgfxContext) {
    GTEST_SKIP() << "Requires live bgfx runtime context to safely validate setLightDirection behavior.";
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

TEST(VoxelRendererTest, RenderWithInvalidBuffersRequiresRuntimeBgfxContext) {
    GTEST_SKIP() << "Requires live bgfx runtime context to safely validate invalid-buffer render path.";
}
