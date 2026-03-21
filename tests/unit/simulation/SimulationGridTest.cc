#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/CellAccessors.hh"
#include "recurse/simulation/VoxelMaterial.hh"
#include <gtest/gtest.h>

using namespace recurse::simulation;

class SimulationGridTest : public ::testing::Test {
  protected:
    SimulationGrid grid;
};

// 1. Write Sand to epoch N+1 write buffer, read epoch N returns Air
TEST_F(SimulationGridTest, ReadWriteIsolation) {
    VoxelCell sand;
    sand = cellForMaterial(material_ids::SAND);

    grid.fillChunk(0, 0, 0, VoxelCell{});
    grid.writeCell(0, 0, 0, sand);

    // Read buffer still has the old value (Air default)
    VoxelCell read = grid.readCell(0, 0, 0);
    EXPECT_EQ(cellMaterialId(read), material_ids::AIR);
}

// 2. Write Sand, advanceEpoch, read returns Sand
TEST_F(SimulationGridTest, AdvanceEpochSwapsBuffers) {
    VoxelCell sand;
    sand = cellForMaterial(material_ids::SAND);

    grid.fillChunk(0, 0, 0, VoxelCell{});
    grid.writeCell(0, 0, 0, sand);
    grid.advanceEpoch();

    VoxelCell read = grid.readCell(0, 0, 0);
    EXPECT_EQ(cellMaterialId(read), material_ids::SAND);
}

// 3. fillChunk 1000 chunks -> materializedChunkCount == 0
TEST_F(SimulationGridTest, HomogeneousSentinelNoAllocation) {
    VoxelCell stone;
    stone = cellForMaterial(material_ids::STONE);

    for (int i = 0; i < 1000; ++i) {
        grid.fillChunk(i, 0, 0, stone);
    }

    EXPECT_EQ(grid.materializedChunkCount(), 0u);
}

// 4. fillChunk(Stone) -> readCell returns Stone
TEST_F(SimulationGridTest, SentinelReadReturnsFillValue) {
    VoxelCell stone;
    stone = cellForMaterial(material_ids::STONE);

    grid.fillChunk(0, 0, 0, stone);

    VoxelCell read = grid.readCell(0, 0, 0);
    EXPECT_EQ(cellMaterialId(read), material_ids::STONE);
}

// 5. Write to sentinel -> isMaterialized, untouched cells keep fill
TEST_F(SimulationGridTest, FirstWritePromotesSentinel) {
    VoxelCell stone;
    stone = cellForMaterial(material_ids::STONE);

    grid.fillChunk(0, 0, 0, stone);
    EXPECT_FALSE(grid.isChunkMaterialized(0, 0, 0));

    VoxelCell sand;
    sand = cellForMaterial(material_ids::SAND);
    grid.writeCell(0, 0, 0, sand);

    EXPECT_TRUE(grid.isChunkMaterialized(0, 0, 0));

    // Untouched cell in same chunk should still have fill value
    // Cell at (1,0,0) was not written to
    grid.advanceEpoch();
    VoxelCell untouched = grid.readCell(1, 0, 0);
    EXPECT_EQ(cellMaterialId(untouched), material_ids::STONE);
}

// 6. static_assert Buffer size == 131072 bytes
TEST_F(SimulationGridTest, MaterializedChunkMemory) {
    static_assert(sizeof(ChunkBuffers::Buffer) == K_CHUNK_VOLUME * sizeof(VoxelCell));
    // K_CHUNK_VOLUME = 32768, sizeof(VoxelCell) = 4 -> 131072
    EXPECT_EQ(sizeof(ChunkBuffers::Buffer), 131072u);
}

// 7. Write at (31,0,0) and (32,0,0), both readable
TEST_F(SimulationGridTest, CrossChunkReads) {
    VoxelCell sand;
    sand = cellForMaterial(material_ids::SAND);

    VoxelCell water;
    water = cellForMaterial(material_ids::WATER);

    grid.writeCell(31, 0, 0, sand);
    grid.writeCell(32, 0, 0, water);
    grid.advanceEpoch();

    EXPECT_EQ(cellMaterialId(grid.readCell(31, 0, 0)), material_ids::SAND);
    EXPECT_EQ(cellMaterialId(grid.readCell(32, 0, 0)), material_ids::WATER);

    // They should be in different chunks
    EXPECT_TRUE(grid.hasChunk(0, 0, 0));
    EXPECT_TRUE(grid.hasChunk(1, 0, 0));
}

// 8. Write (-1,-1,-1) creates chunk (-1,-1,-1) correctly
TEST_F(SimulationGridTest, NegativeCoordinates) {
    VoxelCell dirt;
    dirt = cellForMaterial(material_ids::DIRT);

    grid.writeCell(-1, -1, -1, dirt);
    grid.advanceEpoch();

    EXPECT_EQ(cellMaterialId(grid.readCell(-1, -1, -1)), material_ids::DIRT);
    EXPECT_TRUE(grid.hasChunk(-1, -1, -1));
}

// 9. 10 epochs with writes, reads reflect correct epoch
TEST_F(SimulationGridTest, MultipleEpochCycles) {
    grid.fillChunk(0, 0, 0, VoxelCell{});

    for (uint64_t e = 0; e < 10; ++e) {
        VoxelCell cell = cellForMaterial(static_cast<MaterialId>(e % material_ids::COUNT));
        grid.writeCell(0, 0, 0, cell);
        grid.advanceEpoch();

        VoxelCell read = grid.readCell(0, 0, 0);
        EXPECT_EQ(cellMaterialId(read), static_cast<MaterialId>(e % material_ids::COUNT)) << "Failed at epoch " << e;
    }
    EXPECT_EQ(grid.currentEpoch(), 10u);
}

// 10. readBuffer/writeBuffer match readCell/writeCell
TEST_F(SimulationGridTest, RawBufferMatchesReadWrite) {
    VoxelCell stone;
    stone = cellForMaterial(material_ids::STONE);

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
    EXPECT_EQ(cellMaterialId((*rb)[0]), material_ids::STONE);

    // Should match readCell
    EXPECT_EQ(cellMaterialId(grid.readCell(0, 0, 0)), material_ids::STONE);
}

// 11. writeCellImmediate visible in read buffer without advanceEpoch
TEST_F(SimulationGridTest, WriteCellImmediateVisibleWithoutEpochAdvance) {
    VoxelCell sand;
    sand = cellForMaterial(material_ids::SAND);

    grid.fillChunk(0, 0, 0, VoxelCell{});
    grid.writeCellImmediate(0, 0, 0, sand);

    // Read buffer sees the value immediately -- no advanceEpoch needed
    EXPECT_EQ(cellMaterialId(grid.readCell(0, 0, 0)), material_ids::SAND);

    // Write buffer also has the value
    EXPECT_EQ(cellMaterialId(grid.readFromWriteBuffer(0, 0, 0)), material_ids::SAND);
}

// 12. writeCellImmediate value preserved across advanceEpoch
TEST_F(SimulationGridTest, WriteCellImmediatePreservedAcrossEpoch) {
    VoxelCell sand;
    sand = cellForMaterial(material_ids::SAND);

    grid.fillChunk(0, 0, 0, VoxelCell{});
    grid.writeCellImmediate(0, 0, 0, sand);
    grid.advanceEpoch();

    // Value survives the epoch swap
    EXPECT_EQ(cellMaterialId(grid.readCell(0, 0, 0)), material_ids::SAND);
}

// 13. advanceEpoch preserves untouched cells across K_COUNT * 3 cycles.
// Validates that the copy in advanceEpoch correctly propagates the full state
// into the next write buffer regardless of K_COUNT.
TEST_F(SimulationGridTest, UntouchedCellsSurviveManyCycles) {
    grid.fillChunk(0, 0, 0, VoxelCell{});
    grid.materializeChunk(0, 0, 0);

    VoxelCell stone;
    stone = cellForMaterial(material_ids::STONE);
    grid.writeCell(0, 0, 0, stone);
    grid.advanceEpoch();

    // Run K_COUNT * 3 additional epochs without writing to (0,0,0).
    // The stone value must survive every cycle.
    constexpr int cycles = ChunkBuffers::K_COUNT * 3;
    for (int i = 0; i < cycles; ++i) {
        // Write to a DIFFERENT cell each epoch to exercise the buffers
        VoxelCell sand;
        sand = cellForMaterial(material_ids::SAND);
        grid.writeCell(1, 0, 0, sand);
        grid.advanceEpoch();

        EXPECT_EQ(cellMaterialId(grid.readCell(0, 0, 0)), material_ids::STONE) << "Untouched cell lost at cycle " << i;
    }
}

// 14. K_COUNT buffers are all allocated on materialize
TEST_F(SimulationGridTest, AllBuffersAllocated) {
    grid.fillChunk(0, 0, 0, VoxelCell{});
    grid.materializeChunk(0, 0, 0);

    auto& slot = grid.registry().addChunk(0, 0, 0);
    for (int i = 0; i < ChunkBuffers::K_COUNT; ++i)
        EXPECT_NE(slot.simBuffers.buffers[i], nullptr) << "buffer " << i;
}

// --- Group E: VoxelCell field verification ---

TEST_F(SimulationGridTest, VoxelCellSizeIs4Bytes) {
    static_assert(sizeof(VoxelCell) == 4);
    EXPECT_EQ(sizeof(VoxelCell), 4u);
}

TEST_F(SimulationGridTest, VoxelCellEssenceIdxDefaultZero) {
    VoxelCell cell{};
    EXPECT_EQ(cell.essenceIdx, 0u);
}

TEST_F(SimulationGridTest, VoxelCellEssenceIdxPreservedInCopy) {
    VoxelCell cell{};
    cell = cellForMaterial(material_ids::STONE);
    cell.essenceIdx = 42;
    cell.setFlags(voxel_flags::NONE);

    VoxelCell copy = cell;
    EXPECT_EQ(copy.essenceIdx, 42u);
    EXPECT_EQ(cellMaterialId(copy), static_cast<MaterialId>(42));
}

// --- Group A: Essence round-trip through epoch ---

TEST_F(SimulationGridTest, EssenceIdxPreservedAcrossEpoch) {
    VoxelCell cell{};
    cell = cellForMaterial(material_ids::STONE);
    cell.essenceIdx = 42;

    grid.fillChunk(0, 0, 0, VoxelCell{});
    grid.writeCell(0, 0, 0, cell);
    grid.advanceEpoch();

    VoxelCell read = grid.readCell(0, 0, 0);
    EXPECT_EQ(read.essenceIdx, 42u);
}

TEST_F(SimulationGridTest, EssenceIdxPreservedMultipleEpochs) {
    VoxelCell cell{};
    cell = cellForMaterial(material_ids::STONE);
    cell.essenceIdx = 200;

    grid.fillChunk(0, 0, 0, VoxelCell{});
    grid.writeCell(0, 0, 0, cell);
    grid.advanceEpoch();

    for (int i = 0; i < 10; ++i) {
        VoxelCell other{};
        other = cellForMaterial(material_ids::SAND);
        other.essenceIdx = static_cast<uint8_t>(i);
        grid.writeCell(1, 0, 0, other);
        grid.advanceEpoch();
    }

    VoxelCell read = grid.readCell(0, 0, 0);
    EXPECT_EQ(read.essenceIdx, 200u);
}

TEST_F(SimulationGridTest, EssenceIdxZeroIsDefault) {
    grid.fillChunk(0, 0, 0, VoxelCell{});

    VoxelCell read = grid.readCell(0, 0, 0);
    EXPECT_EQ(read.essenceIdx, 0u);
}

TEST_F(SimulationGridTest, EssenceIdxSurvivesWriteCellImmediate) {
    VoxelCell cell{};
    cell = cellForMaterial(material_ids::STONE);
    cell.essenceIdx = 100;

    grid.fillChunk(0, 0, 0, VoxelCell{});
    grid.writeCellImmediate(0, 0, 0, cell);

    VoxelCell read = grid.readCell(0, 0, 0);
    EXPECT_EQ(read.essenceIdx, 100u);
}
