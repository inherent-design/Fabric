#include <gtest/gtest.h>

#include "fabric/core/VertexPool.hh"

using namespace fabric;

namespace {

VertexPool::Config testConfig(uint32_t buckets = 8) {
    VertexPool::Config cfg;
    cfg.maxVerticesPerBucket = 64;
    cfg.maxIndicesPerBucket = 96;
    cfg.initialBuckets = buckets;
    cfg.cpuOnly = true;
    return cfg;
}

// Fill dummy vertex/index data for allocation tests
void makeDummyMesh(std::vector<VoxelVertex>& verts, std::vector<uint32_t>& indices, uint32_t vertCount,
                   uint32_t idxCount) {
    verts.resize(vertCount);
    indices.resize(idxCount);
    for (uint32_t i = 0; i < vertCount; ++i)
        verts[i] = VoxelVertex::pack(static_cast<uint8_t>(i & 0xFF), 0, 0, 0, 0, 0);
    for (uint32_t i = 0; i < idxCount; ++i)
        indices[i] = i;
}

} // namespace

TEST(VertexPoolTest, NotValidBeforeInit) {
    VertexPool pool;
    EXPECT_FALSE(pool.isValid());
    EXPECT_EQ(pool.allocatedBuckets(), 0u);
    EXPECT_EQ(pool.totalBuckets(), 0u);
}

TEST(VertexPoolTest, InitCreatesValidPool) {
    VertexPool pool;
    pool.init(testConfig());
    EXPECT_TRUE(pool.isValid());
    EXPECT_EQ(pool.totalBuckets(), 8u);
    EXPECT_EQ(pool.allocatedBuckets(), 0u);
}

TEST(VertexPoolTest, AllocateReturnsValidSlot) {
    VertexPool pool;
    pool.init(testConfig());

    std::vector<VoxelVertex> verts;
    std::vector<uint32_t> indices;
    makeDummyMesh(verts, indices, 24, 36);

    PoolSlot slot = pool.allocate(verts.data(), 24, indices.data(), 36);
    EXPECT_TRUE(slot.valid());
    EXPECT_EQ(slot.vertexCount, 24u);
    EXPECT_EQ(slot.indexCount, 36u);
    EXPECT_EQ(pool.allocatedBuckets(), 1u);
}

TEST(VertexPoolTest, AllocateAssignsCorrectOffsets) {
    VertexPool pool;
    auto cfg = testConfig();
    pool.init(cfg);

    std::vector<VoxelVertex> verts;
    std::vector<uint32_t> indices;
    makeDummyMesh(verts, indices, 10, 15);

    PoolSlot s0 = pool.allocate(verts.data(), 10, indices.data(), 15);
    PoolSlot s1 = pool.allocate(verts.data(), 10, indices.data(), 15);

    EXPECT_TRUE(s0.valid());
    EXPECT_TRUE(s1.valid());
    EXPECT_NE(s0.bucketId, s1.bucketId);

    // Offsets should be non-overlapping: each bucket occupies maxVerticesPerBucket
    uint32_t s0End = s0.vertexOffset + cfg.maxVerticesPerBucket;
    uint32_t s1End = s1.vertexOffset + cfg.maxVerticesPerBucket;
    bool noOverlap = (s0End <= s1.vertexOffset) || (s1End <= s0.vertexOffset);
    EXPECT_TRUE(noOverlap);
}

TEST(VertexPoolTest, FreeReturnsSlotForReuse) {
    VertexPool pool;
    pool.init(testConfig());

    std::vector<VoxelVertex> verts;
    std::vector<uint32_t> indices;
    makeDummyMesh(verts, indices, 8, 12);

    PoolSlot slot = pool.allocate(verts.data(), 8, indices.data(), 12);
    EXPECT_EQ(pool.allocatedBuckets(), 1u);

    pool.free(slot);
    EXPECT_EQ(pool.allocatedBuckets(), 0u);

    // Reallocate should get the same bucket back (LIFO free list)
    PoolSlot slot2 = pool.allocate(verts.data(), 8, indices.data(), 12);
    EXPECT_TRUE(slot2.valid());
    EXPECT_EQ(slot2.bucketId, slot.bucketId);
    EXPECT_EQ(pool.allocatedBuckets(), 1u);
}

TEST(VertexPoolTest, AllocFreeCycle) {
    VertexPool pool;
    auto cfg = testConfig(4);
    pool.init(cfg);

    std::vector<VoxelVertex> verts;
    std::vector<uint32_t> indices;
    makeDummyMesh(verts, indices, 4, 6);

    // Allocate all 4 buckets
    PoolSlot slots[4];
    for (int i = 0; i < 4; ++i) {
        slots[i] = pool.allocate(verts.data(), 4, indices.data(), 6);
        EXPECT_TRUE(slots[i].valid());
    }
    EXPECT_EQ(pool.allocatedBuckets(), 4u);

    // Free all
    for (auto& s : slots)
        pool.free(s);
    EXPECT_EQ(pool.allocatedBuckets(), 0u);

    // Reallocate all
    for (int i = 0; i < 4; ++i) {
        slots[i] = pool.allocate(verts.data(), 4, indices.data(), 6);
        EXPECT_TRUE(slots[i].valid());
    }
    EXPECT_EQ(pool.allocatedBuckets(), 4u);
}

TEST(VertexPoolTest, FullPoolReturnsInvalidSlot) {
    VertexPool pool;
    auto cfg = testConfig(2);
    pool.init(cfg);

    std::vector<VoxelVertex> verts;
    std::vector<uint32_t> indices;
    makeDummyMesh(verts, indices, 4, 6);

    PoolSlot s0 = pool.allocate(verts.data(), 4, indices.data(), 6);
    PoolSlot s1 = pool.allocate(verts.data(), 4, indices.data(), 6);
    EXPECT_TRUE(s0.valid());
    EXPECT_TRUE(s1.valid());
    EXPECT_EQ(pool.allocatedBuckets(), 2u);

    // Third allocation should fail
    PoolSlot s2 = pool.allocate(verts.data(), 4, indices.data(), 6);
    EXPECT_FALSE(s2.valid());
    EXPECT_EQ(pool.allocatedBuckets(), 2u);
}

TEST(VertexPoolTest, OversizedMeshReturnsInvalidSlot) {
    VertexPool pool;
    auto cfg = testConfig();
    pool.init(cfg);

    std::vector<VoxelVertex> verts;
    std::vector<uint32_t> indices;

    // Exceed maxVerticesPerBucket (64)
    makeDummyMesh(verts, indices, 65, 36);
    PoolSlot slot = pool.allocate(verts.data(), 65, indices.data(), 36);
    EXPECT_FALSE(slot.valid());

    // Exceed maxIndicesPerBucket (96)
    makeDummyMesh(verts, indices, 24, 97);
    slot = pool.allocate(verts.data(), 24, indices.data(), 97);
    EXPECT_FALSE(slot.valid());

    EXPECT_EQ(pool.allocatedBuckets(), 0u);
}

TEST(VertexPoolTest, ShutdownSafety) {
    VertexPool pool;
    pool.init(testConfig());
    EXPECT_TRUE(pool.isValid());

    pool.shutdown();
    EXPECT_FALSE(pool.isValid());
    EXPECT_EQ(pool.allocatedBuckets(), 0u);

    // Double shutdown is safe
    pool.shutdown();
    EXPECT_FALSE(pool.isValid());
}

TEST(VertexPoolTest, DoubleInitIsNoOp) {
    VertexPool pool;
    auto cfg = testConfig(4);
    pool.init(cfg);
    EXPECT_EQ(pool.totalBuckets(), 4u);

    // Second init with different config should be ignored
    auto cfg2 = testConfig(16);
    pool.init(cfg2);
    EXPECT_EQ(pool.totalBuckets(), 4u);
}

TEST(VertexPoolTest, FreeInvalidSlotIsNoOp) {
    VertexPool pool;
    pool.init(testConfig());

    PoolSlot invalid;
    EXPECT_FALSE(invalid.valid());
    pool.free(invalid); // Should not crash or change state
    EXPECT_EQ(pool.allocatedBuckets(), 0u);
}

TEST(VertexPoolTest, FreeSameSlotTwiceIsNoOp) {
    VertexPool pool;
    pool.init(testConfig());

    std::vector<VoxelVertex> verts;
    std::vector<uint32_t> indices;
    makeDummyMesh(verts, indices, 8, 12);

    PoolSlot slot = pool.allocate(verts.data(), 8, indices.data(), 12);
    pool.free(slot);
    EXPECT_EQ(pool.allocatedBuckets(), 0u);

    // Second free of same slot should be safe
    pool.free(slot);
    EXPECT_EQ(pool.allocatedBuckets(), 0u);
}

TEST(VertexPoolTest, AllocateBeforeInitReturnsInvalid) {
    VertexPool pool;
    std::vector<VoxelVertex> verts;
    std::vector<uint32_t> indices;
    makeDummyMesh(verts, indices, 4, 6);

    PoolSlot slot = pool.allocate(verts.data(), 4, indices.data(), 6);
    EXPECT_FALSE(slot.valid());
}

TEST(VertexPoolTest, ConfigAccessors) {
    VertexPool pool;
    auto cfg = testConfig();
    pool.init(cfg);

    EXPECT_EQ(pool.maxVerticesPerBucket(), 64u);
    EXPECT_EQ(pool.maxIndicesPerBucket(), 96u);
}

TEST(VertexPoolTest, SlotOffsetsAreContiguous) {
    VertexPool pool;
    auto cfg = testConfig(4);
    pool.init(cfg);

    std::vector<VoxelVertex> verts;
    std::vector<uint32_t> indices;
    makeDummyMesh(verts, indices, 4, 6);

    // Allocate all 4 buckets and verify offsets cover the full range
    std::vector<uint32_t> vertexOffsets;
    std::vector<uint32_t> indexOffsets;
    for (int i = 0; i < 4; ++i) {
        PoolSlot s = pool.allocate(verts.data(), 4, indices.data(), 6);
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
