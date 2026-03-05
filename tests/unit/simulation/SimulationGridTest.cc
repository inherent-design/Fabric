#include "fabric/simulation/SimulationGrid.hh"
#include "fabric/simulation/VoxelMaterial.hh"
#include <gtest/gtest.h>

using namespace fabric::simulation;

class SimulationGridTest : public ::testing::Test {
  protected:
    SimulationGrid grid;
};

// 1. Write Sand to epoch N+1 write buffer, read epoch N returns Air
TEST_F(SimulationGridTest, ReadWriteIsolation) {
    VoxelCell sand;
    sand.materialId = MaterialIds::Sand;

    grid.fillChunk(0, 0, 0, VoxelCell{});
    grid.writeCell(0, 0, 0, sand);

    // Read buffer still has the old value (Air default)
    VoxelCell read = grid.readCell(0, 0, 0);
    EXPECT_EQ(read.materialId, MaterialIds::Air);
}

// 2. Write Sand, advanceEpoch, read returns Sand
TEST_F(SimulationGridTest, AdvanceEpochSwapsBuffers) {
    VoxelCell sand;
    sand.materialId = MaterialIds::Sand;

    grid.fillChunk(0, 0, 0, VoxelCell{});
    grid.writeCell(0, 0, 0, sand);
    grid.advanceEpoch();

    VoxelCell read = grid.readCell(0, 0, 0);
    EXPECT_EQ(read.materialId, MaterialIds::Sand);
}

// 3. fillChunk 1000 chunks -> materializedChunkCount == 0
TEST_F(SimulationGridTest, HomogeneousSentinelNoAllocation) {
    VoxelCell stone;
    stone.materialId = MaterialIds::Stone;

    for (int i = 0; i < 1000; ++i) {
        grid.fillChunk(i, 0, 0, stone);
    }

    EXPECT_EQ(grid.materializedChunkCount(), 0u);
}

// 4. fillChunk(Stone) -> readCell returns Stone
TEST_F(SimulationGridTest, SentinelReadReturnsFillValue) {
    VoxelCell stone;
    stone.materialId = MaterialIds::Stone;

    grid.fillChunk(0, 0, 0, stone);

    VoxelCell read = grid.readCell(0, 0, 0);
    EXPECT_EQ(read.materialId, MaterialIds::Stone);
}

// 5. Write to sentinel -> isMaterialized, untouched cells keep fill
TEST_F(SimulationGridTest, FirstWritePromotesSentinel) {
    VoxelCell stone;
    stone.materialId = MaterialIds::Stone;

    grid.fillChunk(0, 0, 0, stone);
    EXPECT_FALSE(grid.isChunkMaterialized(0, 0, 0));

    VoxelCell sand;
    sand.materialId = MaterialIds::Sand;
    grid.writeCell(0, 0, 0, sand);

    EXPECT_TRUE(grid.isChunkMaterialized(0, 0, 0));

    // Untouched cell in same chunk should still have fill value
    // Cell at (1,0,0) was not written to
    grid.advanceEpoch();
    VoxelCell untouched = grid.readCell(1, 0, 0);
    EXPECT_EQ(untouched.materialId, MaterialIds::Stone);
}

// 6. static_assert Buffer size == 131072 bytes
TEST_F(SimulationGridTest, MaterializedChunkMemory) {
    static_assert(sizeof(ChunkBufferPair::Buffer) == kChunkVolume * sizeof(VoxelCell));
    // kChunkVolume = 32768, sizeof(VoxelCell) = 4 -> 131072
    EXPECT_EQ(sizeof(ChunkBufferPair::Buffer), 131072u);
}

// 7. Write at (31,0,0) and (32,0,0), both readable
TEST_F(SimulationGridTest, CrossChunkReads) {
    VoxelCell sand;
    sand.materialId = MaterialIds::Sand;

    VoxelCell water;
    water.materialId = MaterialIds::Water;

    grid.writeCell(31, 0, 0, sand);
    grid.writeCell(32, 0, 0, water);
    grid.advanceEpoch();

    EXPECT_EQ(grid.readCell(31, 0, 0).materialId, MaterialIds::Sand);
    EXPECT_EQ(grid.readCell(32, 0, 0).materialId, MaterialIds::Water);

    // They should be in different chunks
    EXPECT_TRUE(grid.hasChunk(0, 0, 0));
    EXPECT_TRUE(grid.hasChunk(1, 0, 0));
}

// 8. Write (-1,-1,-1) creates chunk (-1,-1,-1) correctly
TEST_F(SimulationGridTest, NegativeCoordinates) {
    VoxelCell dirt;
    dirt.materialId = MaterialIds::Dirt;

    grid.writeCell(-1, -1, -1, dirt);
    grid.advanceEpoch();

    EXPECT_EQ(grid.readCell(-1, -1, -1).materialId, MaterialIds::Dirt);
    EXPECT_TRUE(grid.hasChunk(-1, -1, -1));
}

// 9. 10 epochs with writes, reads reflect correct epoch
TEST_F(SimulationGridTest, MultipleEpochCycles) {
    grid.fillChunk(0, 0, 0, VoxelCell{});

    for (uint64_t e = 0; e < 10; ++e) {
        VoxelCell cell;
        cell.materialId = static_cast<MaterialId>(e % MaterialIds::Count);
        grid.writeCell(0, 0, 0, cell);
        grid.advanceEpoch();

        VoxelCell read = grid.readCell(0, 0, 0);
        EXPECT_EQ(read.materialId, static_cast<MaterialId>(e % MaterialIds::Count)) << "Failed at epoch " << e;
    }
    EXPECT_EQ(grid.currentEpoch(), 10u);
}

// 10. readBuffer/writeBuffer match readCell/writeCell
TEST_F(SimulationGridTest, RawBufferMatchesReadWrite) {
    VoxelCell stone;
    stone.materialId = MaterialIds::Stone;

    grid.fillChunk(0, 0, 0, VoxelCell{});
    grid.materializeChunk(0, 0, 0);

    // Write via writeBuffer
    auto* wb = grid.writeBuffer(0, 0, 0);
    ASSERT_NE(wb, nullptr);
    (*wb)[0] = stone;

    grid.advanceEpoch();

    // Read via readBuffer
    const auto* rb = grid.readBuffer(0, 0, 0);
    ASSERT_NE(rb, nullptr);
    EXPECT_EQ((*rb)[0].materialId, MaterialIds::Stone);

    // Should match readCell
    EXPECT_EQ(grid.readCell(0, 0, 0).materialId, MaterialIds::Stone);
}
