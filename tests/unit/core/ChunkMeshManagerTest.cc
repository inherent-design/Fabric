#include "fabric/core/ChunkMeshManager.hh"
#include <gtest/gtest.h>

using namespace fabric;
using Essence = Vector4<float, Space::World>;

class ChunkMeshManagerTest : public ::testing::Test {
  protected:
    EventDispatcher dispatcher;
    ChunkedGrid<float> density;
    ChunkedGrid<Essence> essence;
};

TEST_F(ChunkMeshManagerTest, MarkDirtyDirect) {
    ChunkMeshManager mgr(dispatcher, density, essence);
    EXPECT_EQ(mgr.dirtyCount(), 0u);
    mgr.markDirty(0, 0, 0);
    EXPECT_EQ(mgr.dirtyCount(), 1u);
    EXPECT_TRUE(mgr.isDirty({0, 0, 0}));
}

TEST_F(ChunkMeshManagerTest, MarkDirtyViaEvent) {
    ChunkMeshManager mgr(dispatcher, density, essence);
    ChunkMeshManager::emitVoxelChanged(dispatcher, 1, 2, 3);
    EXPECT_TRUE(mgr.isDirty({1, 2, 3}));
}

TEST_F(ChunkMeshManagerTest, UpdateRemeshesDirtyChunks) {
    density.set(0, 0, 0, 1.0f);
    ChunkMeshManager mgr(dispatcher, density, essence);
    mgr.markDirty(0, 0, 0);

    int count = mgr.update();
    EXPECT_EQ(count, 1);
    EXPECT_EQ(mgr.dirtyCount(), 0u);

    auto* mesh = mgr.meshFor({0, 0, 0});
    ASSERT_NE(mesh, nullptr);
    EXPECT_EQ(mesh->vertices.size(), 24u);
}

TEST_F(ChunkMeshManagerTest, UpdateRespectsPerTickBudget) {
    ChunkMeshConfig config;
    config.maxRemeshPerTick = 2;
    ChunkMeshManager mgr(dispatcher, density, essence, config);

    mgr.markDirty(0, 0, 0);
    mgr.markDirty(1, 0, 0);
    mgr.markDirty(0, 1, 0);
    EXPECT_EQ(mgr.dirtyCount(), 3u);

    int count = mgr.update();
    EXPECT_EQ(count, 2);
    EXPECT_EQ(mgr.dirtyCount(), 1u);

    count = mgr.update();
    EXPECT_EQ(count, 1);
    EXPECT_EQ(mgr.dirtyCount(), 0u);
}

TEST_F(ChunkMeshManagerTest, MeshForReturnsNullForUnknownChunk) {
    ChunkMeshManager mgr(dispatcher, density, essence);
    EXPECT_EQ(mgr.meshFor({99, 99, 99}), nullptr);
}

TEST_F(ChunkMeshManagerTest, UpdateProducesCorrectGeometry) {
    density.set(0, 0, 0, 1.0f);
    essence.set(0, 0, 0, Essence(0.0f, 1.0f, 0.0f, 0.0f));

    ChunkMeshManager mgr(dispatcher, density, essence);
    mgr.markDirty(0, 0, 0);
    mgr.update();

    auto* mesh = mgr.meshFor({0, 0, 0});
    ASSERT_NE(mesh, nullptr);
    ASSERT_GT(mesh->palette.size(), 0u);

    // Essence is passed through directly as RGBA color
    auto& c = mesh->palette[mesh->vertices[0].paletteIndex()];
    EXPECT_FLOAT_EQ(c[0], 0.0f);
    EXPECT_FLOAT_EQ(c[1], 1.0f);
    EXPECT_FLOAT_EQ(c[2], 0.0f);
}

TEST_F(ChunkMeshManagerTest, RepeatedModificationProducesUpdatedMesh) {
    density.set(0, 0, 0, 1.0f);
    ChunkMeshManager mgr(dispatcher, density, essence);
    mgr.markDirty(0, 0, 0);
    mgr.update();

    auto* mesh1 = mgr.meshFor({0, 0, 0});
    ASSERT_NE(mesh1, nullptr);
    EXPECT_EQ(mesh1->vertices.size(), 24u);

    // Add adjacent voxel, greedy merge keeps same quad count
    density.set(1, 0, 0, 1.0f);
    mgr.markDirty(0, 0, 0);
    mgr.update();

    auto* mesh2 = mgr.meshFor({0, 0, 0});
    ASSERT_NE(mesh2, nullptr);
    EXPECT_EQ(mesh2->vertices.size(), 24u);
    EXPECT_EQ(mesh2->indices.size(), 36u);
}

TEST_F(ChunkMeshManagerTest, EmptyChunkProducesEmptyMesh) {
    ChunkMeshManager mgr(dispatcher, density, essence);
    mgr.markDirty(0, 0, 0);
    mgr.update();

    auto* mesh = mgr.meshFor({0, 0, 0});
    ASSERT_NE(mesh, nullptr);
    EXPECT_EQ(mesh->vertices.size(), 0u);
}

TEST_F(ChunkMeshManagerTest, DeduplicatesDirtyMarking) {
    ChunkMeshManager mgr(dispatcher, density, essence);
    mgr.markDirty(0, 0, 0);
    mgr.markDirty(0, 0, 0);
    mgr.markDirty(0, 0, 0);
    EXPECT_EQ(mgr.dirtyCount(), 1u);
}
