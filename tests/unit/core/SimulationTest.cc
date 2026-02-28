#include "fabric/core/Simulation.hh"
#include <gtest/gtest.h>

using namespace fabric;

TEST(SimulationTest, ConstructEmpty) {
    SimulationHarness sim;
    EXPECT_EQ(sim.ruleCount(), 0u);
    EXPECT_EQ(sim.density().grid().chunkCount(), 0u);
    EXPECT_EQ(sim.essence().grid().chunkCount(), 0u);
}

TEST(SimulationTest, RegisterRule) {
    SimulationHarness sim;
    sim.registerRule("test", [](DensityField&, EssenceField&, int, int, int, double) {});
    EXPECT_EQ(sim.ruleCount(), 1u);
}

TEST(SimulationTest, RemoveRule) {
    SimulationHarness sim;
    sim.registerRule("a", [](DensityField&, EssenceField&, int, int, int, double) {});
    sim.registerRule("b", [](DensityField&, EssenceField&, int, int, int, double) {});
    EXPECT_EQ(sim.ruleCount(), 2u);
    EXPECT_TRUE(sim.removeRule("a"));
    EXPECT_EQ(sim.ruleCount(), 1u);
    EXPECT_FALSE(sim.removeRule("nonexistent"));
}

TEST(SimulationTest, RuleSetsValue) {
    SimulationHarness sim;
    sim.density().write(0, 0, 0, 0.0f);
    sim.registerRule("setter", [](DensityField& d, EssenceField&, int x, int y, int z, double) {
        if (x == 0 && y == 0 && z == 0)
            d.write(0, 0, 0, 1.0f);
    });
    sim.tick(1.0);
    EXPECT_FLOAT_EQ(sim.density().read(0, 0, 0), 1.0f);
}

TEST(SimulationTest, RulesExecuteInOrder) {
    SimulationHarness sim;
    sim.density().write(0, 0, 0, 0.0f);
    sim.registerRule("half", [](DensityField& d, EssenceField&, int x, int y, int z, double) {
        if (x == 0 && y == 0 && z == 0)
            d.write(0, 0, 0, 0.5f);
    });
    sim.registerRule("double", [](DensityField& d, EssenceField&, int x, int y, int z, double) {
        if (x == 0 && y == 0 && z == 0) {
            float v = d.read(0, 0, 0);
            d.write(0, 0, 0, v * 2.0f);
        }
    });
    sim.tick(1.0);
    EXPECT_FLOAT_EQ(sim.density().read(0, 0, 0), 1.0f);
}

TEST(SimulationTest, TickNoActiveChunks) {
    SimulationHarness sim;
    int callCount = 0;
    sim.registerRule("counter", [&](DensityField&, EssenceField&, int, int, int, double) { ++callCount; });
    sim.tick(1.0);
    EXPECT_EQ(callCount, 0);
}

TEST(SimulationTest, NeighborAccess) {
    SimulationHarness sim;
    sim.density().write(5, 5, 5, 1.0f);
    sim.density().write(6, 5, 5, 2.0f);
    sim.density().write(4, 5, 5, 3.0f);

    float neighborSum = 0.0f;
    sim.registerRule("read_neighbors", [&](DensityField& d, EssenceField&, int x, int y, int z, double) {
        if (x == 5 && y == 5 && z == 5) {
            auto n = d.grid().getNeighbors6(5, 5, 5);
            neighborSum = n[0] + n[1]; // +x and -x
        }
    });
    sim.tick(1.0);
    EXPECT_FLOAT_EQ(neighborSum, 5.0f); // 2.0 + 3.0
}

TEST(SimulationTest, EssenceOnlyChunksProcessed) {
    SimulationHarness sim;
    using V4 = Vector4<float, Space::World>;
    sim.essence().write(10, 10, 10, V4(1, 0, 0, 0));

    bool called = false;
    sim.registerRule("detect", [&](DensityField&, EssenceField&, int x, int y, int z, double) {
        if (x == 10 && y == 10 && z == 10)
            called = true;
    });
    sim.tick(1.0);
    EXPECT_TRUE(called);
}

SimRule makeGravityRule() {
    return [](DensityField& d, EssenceField&, int x, int y, int z, double) {
        float here = d.read(x, y, z);
        if (here > 0.0f && y > 0) {
            float below = d.read(x, y - 1, z);
            if (below == 0.0f) {
                d.write(x, y - 1, z, here);
                d.write(x, y, z, 0.0f);
            }
        }
    };
}

TEST(SimulationTest, Gravity) {
    SimulationHarness sim;
    sim.density().write(0, 5, 0, 1.0f);
    sim.registerRule("gravity", makeGravityRule());

    for (int i = 0; i < 5; ++i)
        sim.tick(1.0);

    // After 5 ticks, density should have fallen from y=5 toward y=0
    // With a correct gravity rule, density at (0,5,0) should be 0
    // and density at (0,0,0) should be 1.0
    EXPECT_FLOAT_EQ(sim.density().read(0, 5, 0), 0.0f);
    EXPECT_FLOAT_EQ(sim.density().read(0, 0, 0), 1.0f);
}
