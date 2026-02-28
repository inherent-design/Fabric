#include "fabric/core/WaterSimulation.hh"
#include "fabric/core/ChunkedGrid.hh"
#include "fabric/core/FieldLayer.hh"

#include <gtest/gtest.h>

using namespace fabric;

class WaterSimulationTest : public ::testing::Test {
  protected:
    WaterSimulation sim;
    ChunkedGrid<float> density;

    void TearDown() override { sim.setWaterChangeCallback(nullptr); }
};

TEST_F(WaterSimulationTest, DefaultBudget) {
    EXPECT_EQ(sim.getPerFrameBudget(), 4096);
}

TEST_F(WaterSimulationTest, SetAndGetBudget) {
    sim.setPerFrameBudget(100);
    EXPECT_EQ(sim.getPerFrameBudget(), 100);
}

TEST_F(WaterSimulationTest, WaterFallsDown) {
    // Place water at y=3, empty space below
    sim.waterField().write(0, 3, 0, 1.0f);
    sim.step(density, 0.016f);

    // Water should have moved down (y=2 should have some water)
    float below = sim.waterField().read(0, 2, 0);
    float origin = sim.waterField().read(0, 3, 0);
    EXPECT_GT(below, 0.0f) << "Water should flow downward";
    EXPECT_LT(origin, 1.0f) << "Source should lose water";
}

TEST_F(WaterSimulationTest, WaterStopsOnSolid) {
    // Solid floor at y=0
    density.set(0, 0, 0, 1.0f);
    // Water at y=1
    sim.waterField().write(0, 1, 0, 0.5f);
    sim.step(density, 0.016f);

    // Water should not penetrate the solid
    float inSolid = sim.waterField().read(0, 0, 0);
    EXPECT_FLOAT_EQ(inSolid, 0.0f) << "Water must not enter solid voxels";
}

TEST_F(WaterSimulationTest, LateralSpreadOnFloor) {
    // Solid floor
    for (int x = -2; x <= 2; ++x) {
        for (int z = -2; z <= 2; ++z) {
            density.set(x, 0, z, 1.0f);
        }
    }
    // Water sitting on the floor
    sim.waterField().write(0, 1, 0, 1.0f);

    // Several steps for lateral spread
    for (int i = 0; i < 5; ++i) {
        sim.step(density, 0.016f);
    }

    // Neighbors should have received water
    bool anySpread = false;
    float n1 = sim.waterField().read(1, 1, 0);
    float n2 = sim.waterField().read(-1, 1, 0);
    float n3 = sim.waterField().read(0, 1, 1);
    float n4 = sim.waterField().read(0, 1, -1);
    if (n1 > 0.0f || n2 > 0.0f || n3 > 0.0f || n4 > 0.0f) {
        anySpread = true;
    }
    EXPECT_TRUE(anySpread) << "Water should spread laterally when blocked below";
}

TEST_F(WaterSimulationTest, FillsContainer) {
    // Bowl: solid bottom and walls, open top
    for (int x = -1; x <= 1; ++x) {
        for (int z = -1; z <= 1; ++z) {
            density.set(x, 0, z, 1.0f); // floor
        }
    }
    // Walls
    density.set(-2, 1, 0, 1.0f);
    density.set(2, 1, 0, 1.0f);
    density.set(0, 1, -2, 1.0f);
    density.set(0, 1, 2, 1.0f);

    // Pour water in the center
    sim.waterField().write(0, 1, 0, 1.0f);
    for (int i = 0; i < 20; ++i) {
        sim.step(density, 0.016f);
    }

    // Center should still have water (contained by floor)
    float center = sim.waterField().read(0, 1, 0);
    EXPECT_GT(center, 0.0f) << "Container should hold water";
}

TEST_F(WaterSimulationTest, PressureEqualization) {
    // Solid floor
    for (int x = -3; x <= 3; ++x) {
        density.set(x, 0, 0, 1.0f);
    }
    // High water on left, none on right, connected along y=1
    sim.waterField().write(-2, 1, 0, 1.0f);
    sim.waterField().write(-1, 1, 0, 0.0f);
    sim.waterField().write(0, 1, 0, 0.0f);
    sim.waterField().write(1, 1, 0, 0.0f);
    sim.waterField().write(2, 1, 0, 0.0f);

    for (int i = 0; i < 30; ++i) {
        sim.step(density, 0.016f);
    }

    // After many steps, water levels should roughly equalize
    float left = sim.waterField().read(-2, 1, 0);
    float right = sim.waterField().read(2, 1, 0);
    float diff = std::fabs(left - right);
    EXPECT_LT(diff, 0.5f) << "Water should tend toward pressure equalization";
}

TEST_F(WaterSimulationTest, EmptySpaceNoMovement) {
    // No water anywhere
    sim.step(density, 0.016f);
    EXPECT_EQ(sim.cellsProcessedLastStep(), 0);
}

TEST_F(WaterSimulationTest, PerFrameBudgetRespected) {
    sim.setPerFrameBudget(2);

    // Place water that creates many active cells
    sim.waterField().write(0, 5, 0, 1.0f);
    sim.waterField().write(1, 5, 0, 1.0f);
    sim.waterField().write(2, 5, 0, 1.0f);
    sim.waterField().write(3, 5, 0, 1.0f);

    sim.step(density, 0.016f);

    EXPECT_LE(sim.cellsProcessedLastStep(), 2) << "Budget must cap cells per step";
}

TEST_F(WaterSimulationTest, DoubleBufferSwapCorrectness) {
    sim.waterField().write(0, 3, 0, 1.0f);
    float before = sim.waterField().read(0, 3, 0);
    EXPECT_FLOAT_EQ(before, 1.0f);

    sim.step(density, 0.016f);

    // After step, the "current" buffer should be what was "next"
    // The original position should have less water (some flowed down)
    float after = sim.waterField().read(0, 3, 0);
    EXPECT_LE(after, 1.0f);
}

TEST_F(WaterSimulationTest, WaterChangeEventEmitted) {
    int eventCount = 0;
    sim.setWaterChangeCallback([&](const WaterChangeEvent& e) {
        (void)e;
        ++eventCount;
    });

    sim.waterField().write(0, 3, 0, 1.0f);
    sim.step(density, 0.016f);

    EXPECT_GT(eventCount, 0) << "Change events should fire when water moves";
}

TEST_F(WaterSimulationTest, WaterLevelClampedToMax) {
    // Even if we write > 1.0, after step it should be clamped
    sim.waterField().write(0, 1, 0, 0.8f);
    // Gravity will move water from above into this cell
    sim.waterField().write(0, 2, 0, 0.8f);
    density.set(0, 0, 0, 1.0f); // solid floor

    sim.step(density, 0.016f);

    float level = sim.waterField().read(0, 1, 0);
    EXPECT_LE(level, 1.0f) << "Water level must not exceed 1.0";
}

TEST_F(WaterSimulationTest, WaterDoesNotFlowUpward) {
    density.set(0, 0, 0, 1.0f); // solid floor
    sim.waterField().write(0, 1, 0, 0.5f);

    sim.step(density, 0.016f);

    float above = sim.waterField().read(0, 2, 0);
    EXPECT_FLOAT_EQ(above, 0.0f) << "Water must not flow upward";
}
