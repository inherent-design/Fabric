#include "recurse/simulation/ChunkRegistry.hh"
#include "recurse/simulation/VoxelMaterial.hh"
#include <gtest/gtest.h>

using namespace recurse::simulation;
using fabric::K_CHUNK_VOLUME;

class ChunkRegistryTest : public ::testing::Test {
  protected:
    ChunkRegistry registry;
};

// 1. addChunk returns reference; second call returns same chunk (no duplicate)
TEST_F(ChunkRegistryTest, AddChunkIdempotent) {
    auto& first = registry.addChunk(0, 0, 0);
    first.fillValue.materialId = material_ids::STONE;

    auto& second = registry.addChunk(0, 0, 0);
    EXPECT_EQ(second.fillValue.materialId, material_ids::STONE);
    EXPECT_EQ(&first, &second);
    EXPECT_EQ(registry.chunkCount(), 1u);
}

// 2. find returns nullptr for absent chunk
TEST_F(ChunkRegistryTest, FindAbsentReturnsNull) {
    EXPECT_EQ(registry.find(0, 0, 0), nullptr);

    const auto& constRegistry = registry;
    EXPECT_EQ(constRegistry.find(0, 0, 0), nullptr);
}

// 3. find returns pointer for present chunk
TEST_F(ChunkRegistryTest, FindPresentReturnsPointer) {
    registry.addChunk(1, 2, 3);

    auto* ptr = registry.find(1, 2, 3);
    ASSERT_NE(ptr, nullptr);

    const auto& constRegistry = registry;
    const auto* cptr = constRegistry.find(1, 2, 3);
    ASSERT_NE(cptr, nullptr);
}

// 4. removeChunk makes find return nullptr
TEST_F(ChunkRegistryTest, RemoveChunkMakesAbsent) {
    registry.addChunk(5, 5, 5);
    EXPECT_NE(registry.find(5, 5, 5), nullptr);

    registry.removeChunk(5, 5, 5);
    EXPECT_EQ(registry.find(5, 5, 5), nullptr);
}

// 5. hasChunk true/false
TEST_F(ChunkRegistryTest, HasChunk) {
    EXPECT_FALSE(registry.hasChunk(0, 0, 0));
    registry.addChunk(0, 0, 0);
    EXPECT_TRUE(registry.hasChunk(0, 0, 0));
}

// 6. chunkCount tracks additions/removals
TEST_F(ChunkRegistryTest, ChunkCountTracksChanges) {
    EXPECT_EQ(registry.chunkCount(), 0u);

    registry.addChunk(0, 0, 0);
    EXPECT_EQ(registry.chunkCount(), 1u);

    registry.addChunk(1, 0, 0);
    EXPECT_EQ(registry.chunkCount(), 2u);

    // Duplicate add does not increase count
    registry.addChunk(0, 0, 0);
    EXPECT_EQ(registry.chunkCount(), 2u);

    registry.removeChunk(0, 0, 0);
    EXPECT_EQ(registry.chunkCount(), 1u);
}

// 7. materializedChunkCount only counts materialized
TEST_F(ChunkRegistryTest, MaterializedChunkCount) {
    registry.addChunk(0, 0, 0);
    registry.addChunk(1, 0, 0);
    registry.addChunk(2, 0, 0);

    EXPECT_EQ(registry.materializedChunkCount(), 0u);

    registry.find(0, 0, 0)->materialize();
    EXPECT_EQ(registry.materializedChunkCount(), 1u);

    registry.find(2, 0, 0)->materialize();
    EXPECT_EQ(registry.materializedChunkCount(), 2u);
}

// 8. allChunks returns all chunk coordinates
//    Note: unpackChunkKey has a pre-existing sign-extension bug for negative X;
//    Y and Z negative values roundtrip correctly. Use non-negative X here.
TEST_F(ChunkRegistryTest, AllChunksReturnsCoordinates) {
    registry.addChunk(0, 0, 0);
    registry.addChunk(1, 2, 3);
    registry.addChunk(0, -1, -1);

    auto chunks = registry.allChunks();
    EXPECT_EQ(chunks.size(), 3u);

    auto contains = [&](int cx, int cy, int cz) {
        for (const auto& [x, y, z] : chunks) {
            if (x == cx && y == cy && z == cz)
                return true;
        }
        return false;
    };
    EXPECT_TRUE(contains(0, 0, 0));
    EXPECT_TRUE(contains(1, 2, 3));
    EXPECT_TRUE(contains(0, -1, -1));
}

// 9. clear resets everything
TEST_F(ChunkRegistryTest, ClearResetsAll) {
    registry.addChunk(0, 0, 0);
    registry.addChunk(1, 0, 0);
    registry.find(0, 0, 0)->materialize();

    registry.clear();

    EXPECT_EQ(registry.chunkCount(), 0u);
    EXPECT_EQ(registry.materializedChunkCount(), 0u);
    EXPECT_FALSE(registry.hasChunk(0, 0, 0));
    EXPECT_TRUE(registry.allChunks().empty());
}

// 10. ChunkBufferPair materialize/isMaterialized
TEST_F(ChunkRegistryTest, ChunkBufferPairMaterialize) {
    auto& pair = registry.addChunk(0, 0, 0);
    EXPECT_FALSE(pair.isMaterialized());

    pair.materialize();
    EXPECT_TRUE(pair.isMaterialized());
    ASSERT_NE(pair.buffers[0], nullptr);
    ASSERT_NE(pair.buffers[1], nullptr);

    // Second materialize is a no-op
    auto* buf0 = pair.buffers[0].get();
    pair.materialize();
    EXPECT_EQ(pair.buffers[0].get(), buf0);
}

// 11. Materialized buffers are filled with fillValue
TEST_F(ChunkRegistryTest, MaterializePreservesFillValue) {
    auto& pair = registry.addChunk(0, 0, 0);
    pair.fillValue.materialId = material_ids::STONE;
    pair.materialize();

    for (size_t i = 0; i < K_CHUNK_VOLUME; ++i) {
        EXPECT_EQ((*pair.buffers[0])[i].materialId, material_ids::STONE) << "Buffer 0, index " << i;
        EXPECT_EQ((*pair.buffers[1])[i].materialId, material_ids::STONE) << "Buffer 1, index " << i;
    }
}

// 12. forEachMaterialized only visits materialized chunks
TEST_F(ChunkRegistryTest, ForEachMaterializedSkipsUnmaterialized) {
    registry.addChunk(0, 0, 0);
    registry.addChunk(1, 0, 0);
    registry.addChunk(2, 0, 0);

    registry.find(0, 0, 0)->materialize();
    registry.find(2, 0, 0)->materialize();

    int count = 0;
    registry.forEachMaterialized([&count](ChunkBufferPair&) { ++count; });
    EXPECT_EQ(count, 2);
}

// 13. removeChunk on absent key is safe (no-op)
TEST_F(ChunkRegistryTest, RemoveAbsentChunkIsSafe) {
    EXPECT_NO_THROW(registry.removeChunk(99, 99, 99));
    EXPECT_EQ(registry.chunkCount(), 0u);
}

// 14. Memory size matches expected value
TEST_F(ChunkRegistryTest, BufferMemorySize) {
    static_assert(sizeof(ChunkBufferPair::Buffer) == K_CHUNK_VOLUME * sizeof(VoxelCell));
    EXPECT_EQ(sizeof(ChunkBufferPair::Buffer), 131072u);
}
