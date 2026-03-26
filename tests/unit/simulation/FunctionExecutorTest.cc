#include "recurse/simulation/FunctionExecutor.hh"
#include "recurse/simulation/CellAccessors.hh"
#include "recurse/simulation/ChunkActivityTracker.hh"
#include "recurse/simulation/ChunkFinalization.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include <gtest/gtest.h>

using namespace recurse::simulation;

class FunctionExecutorTest : public ::testing::Test {
  protected:
    SimulationGrid grid;
    ChunkActivityTracker tracker;
    FunctionExecutor executor{grid};

    void SetUp() override {
        grid.fillChunk(0, 0, 0, VoxelCell{});
        grid.materializeChunk(0, 0, 0);
    }

    VoxelCell makeMaterial(MaterialId id) { return cellForMaterial(id); }

    void markActive(ChunkCoord pos) {
        tracker.setState(pos, ChunkState::Active);
        for (int lz = 0; lz < K_CHUNK_SIZE; lz += 8)
            for (int ly = 0; ly < K_CHUNK_SIZE; ly += 8)
                for (int lx = 0; lx < K_CHUNK_SIZE; lx += 8)
                    tracker.markSubRegionActive(pos, lx, ly, lz);
    }
};

// Create writes a cell into an empty location.
TEST_F(FunctionExecutorTest, CreateWritesToEmptyCell) {
    VoxelCell stone = makeMaterial(material_ids::STONE);
    auto result = executor.execute(FunctionOp::Create, 16, 16, 16, stone);
    grid.advanceEpoch();

    EXPECT_EQ(result.cellsModified, 1);
    EXPECT_FALSE(result.budgetExceeded);
    EXPECT_EQ(cellMaterialId(grid.readCell(16, 16, 16)), material_ids::STONE);
}

// Create fails when cell is already occupied.
TEST_F(FunctionExecutorTest, CreateFailsOnOccupiedCell) {
    VoxelCell stone = makeMaterial(material_ids::STONE);
    grid.writeCell(16, 16, 16, stone);
    grid.advanceEpoch();

    VoxelCell water = makeMaterial(material_ids::WATER);
    auto result = executor.execute(FunctionOp::Create, 16, 16, 16, water);

    EXPECT_EQ(result.cellsModified, 0);
    EXPECT_EQ(cellMaterialId(grid.readCell(16, 16, 16)), material_ids::STONE);
}

// Destroy writes an empty cell to an occupied location.
TEST_F(FunctionExecutorTest, DestroyClearsOccupiedCell) {
    VoxelCell stone = makeMaterial(material_ids::STONE);
    grid.writeCell(16, 16, 16, stone);
    grid.advanceEpoch();

    auto result = executor.execute(FunctionOp::Destroy, 16, 16, 16, VoxelCell{});
    grid.advanceEpoch();

    EXPECT_EQ(result.cellsModified, 1);
    EXPECT_TRUE(isEmpty(grid.readCell(16, 16, 16)));
}

// Destroy is a no-op on an already-empty cell.
TEST_F(FunctionExecutorTest, DestroyNoopOnEmptyCell) {
    auto result = executor.execute(FunctionOp::Destroy, 16, 16, 16, VoxelCell{});

    EXPECT_EQ(result.cellsModified, 0);
}

// Transform modifies an existing cell.
TEST_F(FunctionExecutorTest, TransformModifiesExistingCell) {
    VoxelCell water = makeMaterial(material_ids::WATER);
    grid.writeCell(16, 16, 16, water);
    grid.advanceEpoch();

    VoxelCell ice = makeCell(6, Phase::Solid, 90);
    setCellTemperature(ice, 80);
    auto result = executor.execute(FunctionOp::Transform, 16, 16, 16, ice);
    grid.advanceEpoch();

    EXPECT_EQ(result.cellsModified, 1);
    VoxelCell resultCell = grid.readCell(16, 16, 16);
    EXPECT_EQ(resultCell.essenceIdx, 6);
    EXPECT_EQ(resultCell.phase(), Phase::Solid);
}

// Transform is a no-op on an empty cell.
TEST_F(FunctionExecutorTest, TransformNoopOnEmptyCell) {
    VoxelCell stone = makeMaterial(material_ids::STONE);
    auto result = executor.execute(FunctionOp::Transform, 16, 16, 16, stone);

    EXPECT_EQ(result.cellsModified, 0);
    EXPECT_TRUE(isEmpty(grid.readCell(16, 16, 16)));
}

// Budget enforcement: operations stop when budget is exhausted.
TEST_F(FunctionExecutorTest, BudgetEnforcement) {
    FunctionExecutor budgeted{grid, 3};

    VoxelCell stone = makeMaterial(material_ids::STONE);
    for (int i = 0; i < 5; ++i) {
        budgeted.execute(FunctionOp::Create, 16 + i, 16, 16, stone);
    }

    EXPECT_EQ(budgeted.totalModified(), 3);
    EXPECT_EQ(budgeted.budgetRemaining(), 0);
}

// BudgetExceeded flag is set when budget reaches zero.
TEST_F(FunctionExecutorTest, BudgetExceededFlag) {
    FunctionExecutor budgeted{grid, 1};

    VoxelCell stone = makeMaterial(material_ids::STONE);
    auto result1 = budgeted.execute(FunctionOp::Create, 16, 16, 16, stone);
    EXPECT_FALSE(result1.budgetExceeded);

    auto result2 = budgeted.execute(FunctionOp::Create, 17, 16, 16, stone);
    EXPECT_TRUE(result2.budgetExceeded);
    EXPECT_EQ(result2.cellsModified, 0);
}

// resetBudget restores the full budget.
TEST_F(FunctionExecutorTest, ResetBudgetRestoresCapacity) {
    FunctionExecutor budgeted{grid, 2};

    VoxelCell stone = makeMaterial(material_ids::STONE);
    budgeted.execute(FunctionOp::Create, 16, 16, 16, stone);
    budgeted.execute(FunctionOp::Create, 17, 16, 16, stone);
    EXPECT_EQ(budgeted.budgetRemaining(), 0);

    budgeted.resetBudget();
    EXPECT_EQ(budgeted.budgetRemaining(), 2);

    auto result = budgeted.execute(FunctionOp::Create, 18, 16, 16, stone);
    EXPECT_EQ(result.cellsModified, 1);
}

// Per-call budget overrides instance budget.
TEST_F(FunctionExecutorTest, PerCallBudgetOverrides) {
    FunctionExecutor budgeted{grid, 100};

    VoxelCell stone = makeMaterial(material_ids::STONE);
    auto result = budgeted.execute(FunctionOp::Create, 16, 16, 16, stone, 0);
    EXPECT_TRUE(result.budgetExceeded);
    EXPECT_EQ(result.cellsModified, 0);
}

// setMaxCells reduces budget if current budget exceeds new max.
TEST_F(FunctionExecutorTest, SetMaxCellsClampsBudget) {
    FunctionExecutor budgeted{grid, 10};
    budgeted.execute(FunctionOp::Create, 16, 16, 16, makeMaterial(material_ids::STONE));
    EXPECT_EQ(budgeted.budgetRemaining(), 9);

    budgeted.setMaxCells(3);
    EXPECT_EQ(budgeted.budgetRemaining(), 3);
    EXPECT_EQ(budgeted.maxCells(), 3);
}

// Default max cells is 256.
TEST_F(FunctionExecutorTest, DefaultMaxCellsIs256) {
    FunctionExecutor def{grid};
    EXPECT_EQ(def.maxCells(), 256);
}

// Multiple operations accumulate totalModified.
TEST_F(FunctionExecutorTest, TotalModifiedAccumulates) {
    VoxelCell stone = makeMaterial(material_ids::STONE);
    executor.execute(FunctionOp::Create, 16, 16, 16, stone);
    grid.writeCell(17, 16, 16, stone);
    grid.advanceEpoch();
    executor.execute(FunctionOp::Destroy, 17, 16, 16, VoxelCell{});

    EXPECT_EQ(executor.totalModified(), 2);
}

// Temperature is preserved through Transform.
TEST_F(FunctionExecutorTest, TemperaturePreservedThroughTransform) {
    VoxelCell water = makeMaterial(material_ids::WATER);
    setCellTemperature(water, 42);
    grid.writeCell(16, 16, 16, water);
    grid.advanceEpoch();

    VoxelCell ice = makeCell(6, Phase::Solid, 90);
    setCellTemperature(ice, 80);
    executor.execute(FunctionOp::Transform, 16, 16, 16, ice);
    grid.advanceEpoch();

    VoxelCell resultCell = grid.readCell(16, 16, 16);
    EXPECT_EQ(cellTemperature(resultCell), 80);
}
