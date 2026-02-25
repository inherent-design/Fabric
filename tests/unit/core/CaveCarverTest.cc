#include "fabric/core/CaveCarver.hh"
#include <gtest/gtest.h>

using namespace fabric;

class CaveCarverTest : public ::testing::Test {
  protected:
    CaveConfig defaultConfig;

    void SetUp() override {
        defaultConfig.seed = 42;
        defaultConfig.frequency = 0.05f;
        defaultConfig.threshold = 0.3f;
        defaultConfig.worminess = 1.0f;
        defaultConfig.minRadius = 1.0f;
        defaultConfig.maxRadius = 3.0f;
    }
};

TEST_F(CaveCarverTest, CarvingReducesDensity) {
    CaveCarver carver(defaultConfig);

    FieldLayer<float> density;
    AABB region(Vec3f(0.0f, 0.0f, 0.0f), Vec3f(16.0f, 16.0f, 16.0f));

    // Fill density to 1.0 (solid) before carving
    density.fill(0, 0, 0, 15, 15, 15, 1.0f);

    carver.carve(density, region);

    // At least some voxels should have reduced density
    int carved = 0;
    for (int z = 0; z < 16; ++z) {
        for (int y = 0; y < 16; ++y) {
            for (int x = 0; x < 16; ++x) {
                if (density.read(x, y, z) < 1.0f) {
                    ++carved;
                }
            }
        }
    }
    EXPECT_GT(carved, 0) << "Carving should reduce density of at least some voxels";
}

TEST_F(CaveCarverTest, DensityNeverNegative) {
    CaveCarver carver(defaultConfig);

    FieldLayer<float> density;
    AABB region(Vec3f(0.0f, 0.0f, 0.0f), Vec3f(16.0f, 16.0f, 16.0f));
    density.fill(0, 0, 0, 15, 15, 15, 0.5f);

    carver.carve(density, region);

    for (int z = 0; z < 16; ++z) {
        for (int y = 0; y < 16; ++y) {
            for (int x = 0; x < 16; ++x) {
                EXPECT_GE(density.read(x, y, z), 0.0f) << "at (" << x << "," << y << "," << z << ")";
            }
        }
    }
}

TEST_F(CaveCarverTest, UncarvableRegionPreserved) {
    // Setting a very high threshold means almost nothing gets carved
    CaveConfig strictConfig = defaultConfig;
    strictConfig.threshold = 0.99f;
    CaveCarver carver(strictConfig);

    FieldLayer<float> density;
    AABB region(Vec3f(0.0f, 0.0f, 0.0f), Vec3f(8.0f, 8.0f, 8.0f));
    density.fill(0, 0, 0, 7, 7, 7, 1.0f);

    carver.carve(density, region);

    int unchanged = 0;
    for (int z = 0; z < 8; ++z) {
        for (int y = 0; y < 8; ++y) {
            for (int x = 0; x < 8; ++x) {
                if (density.read(x, y, z) == 1.0f) {
                    ++unchanged;
                }
            }
        }
    }
    // Most voxels should remain unchanged with 0.99 threshold
    EXPECT_GT(unchanged, 400) << "High threshold should preserve most density";
}

TEST_F(CaveCarverTest, DifferentSeedsProduceDifferentCaves) {
    FieldLayer<float> densityA;
    FieldLayer<float> densityB;
    AABB region(Vec3f(0.0f, 0.0f, 0.0f), Vec3f(8.0f, 8.0f, 8.0f));

    densityA.fill(0, 0, 0, 7, 7, 7, 1.0f);
    densityB.fill(0, 0, 0, 7, 7, 7, 1.0f);

    CaveConfig cfgA = defaultConfig;
    cfgA.seed = 100;
    CaveCarver carverA(cfgA);
    carverA.carve(densityA, region);

    CaveConfig cfgB = defaultConfig;
    cfgB.seed = 999;
    CaveCarver carverB(cfgB);
    carverB.carve(densityB, region);

    bool anyDifferent = false;
    for (int z = 0; z < 8 && !anyDifferent; ++z) {
        for (int y = 0; y < 8 && !anyDifferent; ++y) {
            for (int x = 0; x < 8 && !anyDifferent; ++x) {
                if (densityA.read(x, y, z) != densityB.read(x, y, z)) {
                    anyDifferent = true;
                }
            }
        }
    }
    EXPECT_TRUE(anyDifferent) << "Different seeds should produce different cave patterns";
}

TEST_F(CaveCarverTest, SameSeedProducesSameResult) {
    FieldLayer<float> densityA;
    FieldLayer<float> densityB;
    AABB region(Vec3f(0.0f, 0.0f, 0.0f), Vec3f(8.0f, 8.0f, 8.0f));

    densityA.fill(0, 0, 0, 7, 7, 7, 1.0f);
    densityB.fill(0, 0, 0, 7, 7, 7, 1.0f);

    CaveCarver carverA(defaultConfig);
    carverA.carve(densityA, region);

    CaveCarver carverB(defaultConfig);
    carverB.carve(densityB, region);

    for (int z = 0; z < 8; ++z) {
        for (int y = 0; y < 8; ++y) {
            for (int x = 0; x < 8; ++x) {
                EXPECT_FLOAT_EQ(densityA.read(x, y, z), densityB.read(x, y, z))
                    << "Same seed should produce identical caves at (" << x << "," << y << "," << z << ")";
            }
        }
    }
}

TEST_F(CaveCarverTest, RegionBoundsRespected) {
    CaveCarver carver(defaultConfig);

    FieldLayer<float> density;
    // Fill a larger area
    density.fill(-8, -8, -8, 23, 23, 23, 1.0f);

    // Carve only in the sub-region [4, 12)
    AABB region(Vec3f(4.0f, 4.0f, 4.0f), Vec3f(12.0f, 12.0f, 12.0f));
    carver.carve(density, region);

    // Voxels outside the region should be unchanged (1.0)
    bool outsideChanged = false;
    for (int z = -8; z < 24; ++z) {
        for (int y = -8; y < 24; ++y) {
            for (int x = -8; x < 24; ++x) {
                bool inRegion = (x >= 4 && x < 12 && y >= 4 && y < 12 && z >= 4 && z < 12);
                if (!inRegion && density.read(x, y, z) != 1.0f) {
                    outsideChanged = true;
                }
            }
        }
    }
    EXPECT_FALSE(outsideChanged) << "Carving should only affect voxels within the specified region";
}

TEST_F(CaveCarverTest, EmptyRegionDoesNothing) {
    CaveCarver carver(defaultConfig);

    FieldLayer<float> density;
    AABB region(Vec3f(5.0f, 5.0f, 5.0f), Vec3f(5.0f, 5.0f, 5.0f));
    carver.carve(density, region);
    EXPECT_EQ(density.grid().chunkCount(), 0u);
}

TEST_F(CaveCarverTest, ConfigAccessors) {
    CaveConfig cfg;
    cfg.seed = 7;
    cfg.frequency = 0.1f;
    CaveCarver carver(cfg);

    EXPECT_EQ(carver.config().seed, 7);
    EXPECT_FLOAT_EQ(carver.config().frequency, 0.1f);

    CaveConfig newCfg;
    newCfg.seed = 42;
    carver.setConfig(newCfg);
    EXPECT_EQ(carver.config().seed, 42);
}

TEST_F(CaveCarverTest, HighWorminessProducesMoreTunnels) {
    FieldLayer<float> densityLow;
    FieldLayer<float> densityHigh;
    AABB region(Vec3f(0.0f, 0.0f, 0.0f), Vec3f(16.0f, 16.0f, 16.0f));

    densityLow.fill(0, 0, 0, 15, 15, 15, 1.0f);
    densityHigh.fill(0, 0, 0, 15, 15, 15, 1.0f);

    CaveConfig lowWorm = defaultConfig;
    lowWorm.worminess = 0.1f;
    CaveCarver carverLow(lowWorm);
    carverLow.carve(densityLow, region);

    CaveConfig highWorm = defaultConfig;
    highWorm.worminess = 5.0f;
    CaveCarver carverHigh(highWorm);
    carverHigh.carve(densityHigh, region);

    // Different worminess should produce different patterns
    bool anyDifferent = false;
    for (int z = 0; z < 16 && !anyDifferent; ++z) {
        for (int y = 0; y < 16 && !anyDifferent; ++y) {
            for (int x = 0; x < 16 && !anyDifferent; ++x) {
                if (densityLow.read(x, y, z) != densityHigh.read(x, y, z)) {
                    anyDifferent = true;
                }
            }
        }
    }
    EXPECT_TRUE(anyDifferent) << "Different worminess should produce different cave patterns";
}

TEST_F(CaveCarverTest, NegativeRegionCoordinates) {
    CaveCarver carver(defaultConfig);

    FieldLayer<float> density;
    AABB region(Vec3f(-8.0f, -8.0f, -8.0f), Vec3f(0.0f, 0.0f, 0.0f));
    density.fill(-8, -8, -8, -1, -1, -1, 1.0f);

    carver.carve(density, region);

    // Should have carved some voxels
    int carved = 0;
    for (int z = -8; z < 0; ++z) {
        for (int y = -8; y < 0; ++y) {
            for (int x = -8; x < 0; ++x) {
                if (density.read(x, y, z) < 1.0f) {
                    ++carved;
                }
            }
        }
    }
    EXPECT_GT(carved, 0);
}
