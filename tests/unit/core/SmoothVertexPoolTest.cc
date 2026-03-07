#include <gtest/gtest.h>

#include <algorithm>

#include "recurse/render/VertexPool.hh"

using namespace recurse;

namespace {

SmoothVertexPool::Config testConfig(uint32_t buckets = 8) {
    SmoothVertexPool::Config cfg;
    cfg.maxVerticesPerBucket = 64;
    cfg.maxIndicesPerBucket = 96;
    cfg.initialBuckets = buckets;
    cfg.cpuOnly = true;
    return cfg;
}

void makeDummyMesh(std::vector<SmoothVoxelVertex>& verts, std::vector<uint32_t>& indices, uint32_t vertCount,
                   uint32_t idxCount) {
    verts.resize(vertCount);
    indices.resize(idxCount);
    for (uint32_t i = 0; i < vertCount; ++i)
        verts[i] = {static_cast<float>(i), 0, 0, 0, 1, 0, 0, 0};
    for (uint32_t i = 0; i < idxCount; ++i)
        indices[i] = i;
}

} // namespace

TEST(SmoothVertexPoolTest, NotValidBeforeInit) {
    SmoothVertexPool pool;
    EXPECT_FALSE(pool.isValid());
    EXPECT_EQ(pool.allocatedBuckets(), 0u);
    EXPECT_EQ(pool.totalBuckets(), 0u);
}

TEST(SmoothVertexPoolTest, InitCreatesValidPool) {
    SmoothVertexPool pool;
    pool.init(testConfig());
    EXPECT_TRUE(pool.isValid());
    EXPECT_EQ(pool.totalBuckets(), 8u);
    EXPECT_EQ(pool.allocatedBuckets(), 0u);
}

TEST(SmoothVertexPoolTest, AllocateReturnsValidSlot) {
    SmoothVertexPool pool;
    pool.init(testConfig());

    std::vector<SmoothVoxelVertex> verts;
    std::vector<uint32_t> indices;
    makeDummyMesh(verts, indices, 24, 36);

    SmoothPoolSlot slot = pool.allocate(verts.data(), 24, indices.data(), 36);
    EXPECT_TRUE(slot.valid());
    EXPECT_EQ(slot.vertexCount, 24u);
    EXPECT_EQ(slot.indexCount, 36u);
    EXPECT_EQ(pool.allocatedBuckets(), 1u);
}

TEST(SmoothVertexPoolTest, FreeReturnsSlotForReuse) {
    SmoothVertexPool pool;
    pool.init(testConfig());

    std::vector<SmoothVoxelVertex> verts;
    std::vector<uint32_t> indices;
    makeDummyMesh(verts, indices, 8, 12);

    SmoothPoolSlot slot = pool.allocate(verts.data(), 8, indices.data(), 12);
    EXPECT_EQ(pool.allocatedBuckets(), 1u);

    pool.free(slot);
    EXPECT_EQ(pool.allocatedBuckets(), 0u);

    // Reallocate should get the same bucket back (LIFO free list)
    SmoothPoolSlot slot2 = pool.allocate(verts.data(), 8, indices.data(), 12);
    EXPECT_TRUE(slot2.valid());
    EXPECT_EQ(slot2.bucketId, slot.bucketId);
    EXPECT_EQ(pool.allocatedBuckets(), 1u);
}

TEST(SmoothVertexPoolTest, FullPoolReturnsInvalidSlot) {
    SmoothVertexPool pool;
    auto cfg = testConfig(2);
    pool.init(cfg);

    std::vector<SmoothVoxelVertex> verts;
    std::vector<uint32_t> indices;
    makeDummyMesh(verts, indices, 4, 6);

    SmoothPoolSlot s0 = pool.allocate(verts.data(), 4, indices.data(), 6);
    SmoothPoolSlot s1 = pool.allocate(verts.data(), 4, indices.data(), 6);
    EXPECT_TRUE(s0.valid());
    EXPECT_TRUE(s1.valid());
    EXPECT_EQ(pool.allocatedBuckets(), 2u);

    // Third allocation should fail
    SmoothPoolSlot s2 = pool.allocate(verts.data(), 4, indices.data(), 6);
    EXPECT_FALSE(s2.valid());
    EXPECT_EQ(pool.allocatedBuckets(), 2u);
}

TEST(SmoothVertexPoolTest, OversizedMeshReturnsInvalid) {
    SmoothVertexPool pool;
    auto cfg = testConfig();
    pool.init(cfg);

    std::vector<SmoothVoxelVertex> verts;
    std::vector<uint32_t> indices;

    // Exceed maxVerticesPerBucket (64)
    makeDummyMesh(verts, indices, 65, 36);
    SmoothPoolSlot slot = pool.allocate(verts.data(), 65, indices.data(), 36);
    EXPECT_FALSE(slot.valid());

    // Exceed maxIndicesPerBucket (96)
    makeDummyMesh(verts, indices, 24, 97);
    slot = pool.allocate(verts.data(), 24, indices.data(), 97);
    EXPECT_FALSE(slot.valid());

    EXPECT_EQ(pool.allocatedBuckets(), 0u);
}

TEST(SmoothVertexPoolTest, BucketSizeParity) {
    // Production config: 4K verts * 32B = 128KB, matching VertexPool's 16K * 8B
    SmoothVertexPool::Config cfg;
    EXPECT_EQ(cfg.maxVerticesPerBucket * sizeof(SmoothVoxelVertex), 128u * 1024u);
}

TEST(SmoothVertexPoolTest, SlotOffsetsAreContiguous) {
    SmoothVertexPool pool;
    auto cfg = testConfig(4);
    pool.init(cfg);

    std::vector<SmoothVoxelVertex> verts;
    std::vector<uint32_t> indices;
    makeDummyMesh(verts, indices, 4, 6);

    // Allocate all 4 buckets and verify offsets cover the full range
    std::vector<uint32_t> vertexOffsets;
    std::vector<uint32_t> indexOffsets;
    for (int i = 0; i < 4; ++i) {
        SmoothPoolSlot s = pool.allocate(verts.data(), 4, indices.data(), 6);
        vertexOffsets.push_back(s.vertexOffset);
        indexOffsets.push_back(s.indexOffset);
    }

    // Sort and verify non-overlapping, contiguous
    std::sort(vertexOffsets.begin(), vertexOffsets.end());
    std::sort(indexOffsets.begin(), indexOffsets.end());

    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(vertexOffsets[i], static_cast<uint32_t>(i) * cfg.maxVerticesPerBucket);
        EXPECT_EQ(indexOffsets[i], static_cast<uint32_t>(i) * cfg.maxIndicesPerBucket);
    }
}

TEST(SmoothVertexPoolTest, ShutdownSafety) {
    SmoothVertexPool pool;
    pool.init(testConfig());
    EXPECT_TRUE(pool.isValid());

    pool.shutdown();
    EXPECT_FALSE(pool.isValid());
    EXPECT_EQ(pool.allocatedBuckets(), 0u);

    // Double shutdown is safe
    pool.shutdown();
    EXPECT_FALSE(pool.isValid());
}
