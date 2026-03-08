#include "fabric/simulation/SimulationGrid.hh"
#include "fabric/simulation/VoxelMaterial.hh"
#include <gtest/gtest.h>

using namespace fabric::simulation;
using fabric::K_CHUNK_VOLUME;

class SimulationGridTest : public ::testing::Test {
  protected:
    SimulationGrid grid;
};

// 1. Write Sand to epoch N+1 write buffer, read epoch N returns Air
TEST_F(SimulationGridTest, ReadWriteIsolation) {
    VoxelCell sand;
    sand.materialId = material_ids::SAND;

    grid.fillChunk(0, 0, 0, VoxelCell{});
    grid.writeCell(0, 0, 0, sand);

    // Read buffer still has the old value (Air default)
    VoxelCell read = grid.readCell(0, 0, 0);
    EXPECT_EQ(read.materialId, material_ids::AIR);
}

// 2. Write Sand, advanceEpoch, read returns Sand
TEST_F(SimulationGridTest, AdvanceEpochSwapsBuffers) {
    VoxelCell sand;
    sand.materialId = material_ids::SAND;

    grid.fillChunk(0, 0, 0, VoxelCell{});
    grid.writeCell(0, 0, 0, sand);
    grid.advanceEpoch();

    VoxelCell read = grid.readCell(0, 0, 0);
    EXPECT_EQ(read.materialId, material_ids::SAND);
}

// 3. fillChunk 1000 chunks -> materializedChunkCount == 0
TEST_F(SimulationGridTest, HomogeneousSentinelNoAllocation) {
    VoxelCell stone;
    stone.materialId = material_ids::STONE;

    for (int i = 0; i < 1000; ++i) {
        grid.fillChunk(i, 0, 0, stone);
    }

    EXPECT_EQ(grid.materializedChunkCount(), 0u);
}

// 4. fillChunk(Stone) -> readCell returns Stone
TEST_F(SimulationGridTest, SentinelReadReturnsFillValue) {
    VoxelCell stone;
    stone.materialId = material_ids::STONE;

    grid.fillChunk(0, 0, 0, stone);

    VoxelCell read = grid.readCell(0, 0, 0);
    EXPECT_EQ(read.materialId, material_ids::STONE);
}

// 5. Write to sentinel -> isMaterialized, untouched cells keep fill
TEST_F(SimulationGridTest, FirstWritePromotesSentinel) {
    VoxelCell stone;
    stone.materialId = material_ids::STONE;

    grid.fillChunk(0, 0, 0, stone);
    EXPECT_FALSE(grid.isChunkMaterialized(0, 0, 0));

    VoxelCell sand;
    sand.materialId = material_ids::SAND;
    grid.writeCell(0, 0, 0, sand);

    EXPECT_TRUE(grid.isChunkMaterialized(0, 0, 0));

    // Untouched cell in same chunk should still have fill value
    // Cell at (1,0,0) was not written to
    grid.advanceEpoch();
    VoxelCell untouched = grid.readCell(1, 0, 0);
    EXPECT_EQ(untouched.materialId, material_ids::STONE);
}

// 6. static_assert Buffer size == 131072 bytes
TEST_F(SimulationGridTest, MaterializedChunkMemory) {
    static_assert(sizeof(ChunkBufferPair::Buffer) == K_CHUNK_VOLUME * sizeof(VoxelCell));
    // K_CHUNK_VOLUME = 32768, sizeof(VoxelCell) = 4 -> 131072
    EXPECT_EQ(sizeof(ChunkBufferPair::Buffer), 131072u);
}

// 7. Write at (31,0,0) and (32,0,0), both readable
TEST_F(SimulationGridTest, CrossChunkReads) {
    VoxelCell sand;
    sand.materialId = material_ids::SAND;

    VoxelCell water;
    water.materialId = material_ids::WATER;

    grid.writeCell(31, 0, 0, sand);
    grid.writeCell(32, 0, 0, water);
    grid.advanceEpoch();

    EXPECT_EQ(grid.readCell(31, 0, 0).materialId, material_ids::SAND);
    EXPECT_EQ(grid.readCell(32, 0, 0).materialId, material_ids::WATER);

    // They should be in different chunks
    EXPECT_TRUE(grid.hasChunk(0, 0, 0));
    EXPECT_TRUE(grid.hasChunk(1, 0, 0));
}

// 8. Write (-1,-1,-1) creates chunk (-1,-1,-1) correctly
TEST_F(SimulationGridTest, NegativeCoordinates) {
    VoxelCell dirt;
    dirt.materialId = material_ids::DIRT;

    grid.writeCell(-1, -1, -1, dirt);
    grid.advanceEpoch();

    EXPECT_EQ(grid.readCell(-1, -1, -1).materialId, material_ids::DIRT);
    EXPECT_TRUE(grid.hasChunk(-1, -1, -1));
}

// 9. 10 epochs with writes, reads reflect correct epoch
TEST_F(SimulationGridTest, MultipleEpochCycles) {
    grid.fillChunk(0, 0, 0, VoxelCell{});

    for (uint64_t e = 0; e < 10; ++e) {
        VoxelCell cell;
        cell.materialId = static_cast<MaterialId>(e % material_ids::COUNT);
        grid.writeCell(0, 0, 0, cell);
        grid.advanceEpoch();

        VoxelCell read = grid.readCell(0, 0, 0);
        EXPECT_EQ(read.materialId, static_cast<MaterialId>(e % material_ids::COUNT)) << "Failed at epoch " << e;
    }
    EXPECT_EQ(grid.currentEpoch(), 10u);
}

// 10. readBuffer/writeBuffer match readCell/writeCell
TEST_F(SimulationGridTest, RawBufferMatchesReadWrite) {
    VoxelCell stone;
    stone.materialId = material_ids::STONE;

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
    EXPECT_EQ((*rb)[0].materialId, material_ids::STONE);

    // Should match readCell
    EXPECT_EQ(grid.readCell(0, 0, 0).materialId, material_ids::STONE);
}

// 11. writeCellImmediate visible in read buffer without advanceEpoch
TEST_F(SimulationGridTest, WriteCellImmediateVisibleWithoutEpochAdvance) {
    VoxelCell sand;
    sand.materialId = material_ids::SAND;

    grid.fillChunk(0, 0, 0, VoxelCell{});
    grid.writeCellImmediate(0, 0, 0, sand);

    // Read buffer sees the value immediately -- no advanceEpoch needed
    EXPECT_EQ(grid.readCell(0, 0, 0).materialId, material_ids::SAND);

    // Write buffer also has the value
    EXPECT_EQ(grid.readFromWriteBuffer(0, 0, 0).materialId, material_ids::SAND);
}

// 12. writeCellImmediate value preserved across advanceEpoch
TEST_F(SimulationGridTest, WriteCellImmediatePreservedAcrossEpoch) {
    VoxelCell sand;
    sand.materialId = material_ids::SAND;

    grid.fillChunk(0, 0, 0, VoxelCell{});
    grid.writeCellImmediate(0, 0, 0, sand);
    grid.advanceEpoch();

    // Value survives the epoch swap
    EXPECT_EQ(grid.readCell(0, 0, 0).materialId, material_ids::SAND);
}
