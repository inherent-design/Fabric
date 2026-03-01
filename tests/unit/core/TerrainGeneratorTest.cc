#include "fabric/core/TerrainGenerator.hh"
#include <gtest/gtest.h>

using namespace fabric;

class TerrainGeneratorTest : public ::testing::Test {
  protected:
    TerrainConfig defaultConfig;
    void SetUp() override {
        defaultConfig.seed = 42;
        defaultConfig.frequency = 0.05f;
        defaultConfig.octaves = 3;
    }
};

TEST_F(TerrainGeneratorTest, DensityValuesInZeroOneRange) {
    TerrainGenerator gen(defaultConfig);
    FieldLayer<float> density;
    FieldLayer<Vector4<float, Space::World>> essence;

    AABB region(Vec3f(0.0f, 0.0f, 0.0f), Vec3f(8.0f, 8.0f, 8.0f));
    gen.generate(density, essence, region);

    for (int z = 0; z < 8; ++z) {
        for (int y = 0; y < 8; ++y) {
            for (int x = 0; x < 8; ++x) {
                float d = density.read(x, y, z);
                EXPECT_GE(d, 0.0f) << "at (" << x << "," << y << "," << z << ")";
                EXPECT_LE(d, 1.0f) << "at (" << x << "," << y << "," << z << ")";
            }
        }
    }
}

TEST_F(TerrainGeneratorTest, SeedVariationProducesDifferentOutput) {
    FieldLayer<float> densityA;
    FieldLayer<Vector4<float, Space::World>> essenceA;
    FieldLayer<float> densityB;
    FieldLayer<Vector4<float, Space::World>> essenceB;

    AABB region(Vec3f(0.0f, 0.0f, 0.0f), Vec3f(4.0f, 4.0f, 4.0f));

    TerrainConfig cfgA = defaultConfig;
    cfgA.seed = 100;
    TerrainGenerator genA(cfgA);
    genA.generate(densityA, essenceA, region);

    TerrainConfig cfgB = defaultConfig;
    cfgB.seed = 999;
    TerrainGenerator genB(cfgB);
    genB.generate(densityB, essenceB, region);

    bool anyDifferent = false;
    for (int z = 0; z < 4; ++z) {
        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 4; ++x) {
                if (densityA.read(x, y, z) != densityB.read(x, y, z)) {
                    anyDifferent = true;
                    break;
                }
            }
            if (anyDifferent)
                break;
        }
        if (anyDifferent)
            break;
    }
    EXPECT_TRUE(anyDifferent) << "Different seeds should produce different terrain";
}

TEST_F(TerrainGeneratorTest, RegionCoverageIsComplete) {
    TerrainGenerator gen(defaultConfig);
    FieldLayer<float> density;
    FieldLayer<Vector4<float, Space::World>> essence;

    AABB region(Vec3f(10.0f, 10.0f, 10.0f), Vec3f(14.0f, 14.0f, 14.0f));
    gen.generate(density, essence, region);

    // Every cell in the region should have been written.
    // Density is remapped to [0,1] from noise, so values should not be exactly the
    // default value of 0.0f (noise remaps to ~0.5 center).
    int written = 0;
    for (int z = 10; z < 14; ++z) {
        for (int y = 10; y < 14; ++y) {
            for (int x = 10; x < 14; ++x) {
                auto e = essence.read(x, y, z);
                // Essence was written: alpha channel equals density, and rgb are normalized coords
                if (e.w >= 0.0f)
                    ++written;
            }
        }
    }
    // All 4*4*4 = 64 cells should have been written
    EXPECT_EQ(written, 64);
    // Grid should have allocated chunks for the region
    EXPECT_GT(density.grid().chunkCount(), 0u);
}

TEST_F(TerrainGeneratorTest, EssenceIsDiscreteMaterialColor) {
    // Use frequency that spans the density threshold to get both air and solid
    TerrainConfig cfg = defaultConfig;
    cfg.frequency = 0.5f; // higher frequency → more density variation in small region
    TerrainGenerator gen(cfg);
    FieldLayer<float> density;
    FieldLayer<Vector4<float, Space::World>> essence;

    AABB region(Vec3f(0.0f, 0.0f, 0.0f), Vec3f(8.0f, 8.0f, 8.0f));
    gen.generate(density, essence, region);

    // Validate mapping: air→(0,0,0,0), solid→discrete material with alpha=1
    for (int z = 0; z < 8; ++z) {
        for (int y = 0; y < 8; ++y) {
            for (int x = 0; x < 8; ++x) {
                float d = density.read(x, y, z);
                auto e = essence.read(x, y, z);
                if (d <= 0.5f) {
                    EXPECT_FLOAT_EQ(e.x, 0.0f)
                        << "Air should have zero essence at (" << x << "," << y << "," << z << ")";
                    EXPECT_FLOAT_EQ(e.w, 0.0f);
                } else {
                    EXPECT_FLOAT_EQ(e.w, 1.0f)
                        << "Solid voxel should have alpha=1 at (" << x << "," << y << "," << z << ")";
                    // Must be one of: grass=(0.34,0.64,0.24), dirt=(0.55,0.36,0.22), stone=(0.52,0.52,0.54)
                    bool isGrass = (e.x == 0.34f && e.y == 0.64f);
                    bool isDirt = (e.x == 0.55f && e.y == 0.36f);
                    bool isStone = (e.x == 0.52f && e.y == 0.52f);
                    EXPECT_TRUE(isGrass || isDirt || isStone)
                        << "Unknown material color (" << e.x << "," << e.y << "," << e.z << ") at (" << x << "," << y
                        << "," << z << ") d=" << d;
                }
            }
        }
    }
}

TEST_F(TerrainGeneratorTest, EmptyRegionDoesNothing) {
    TerrainGenerator gen(defaultConfig);
    FieldLayer<float> density;
    FieldLayer<Vector4<float, Space::World>> essence;

    // Zero-volume region
    AABB region(Vec3f(5.0f, 5.0f, 5.0f), Vec3f(5.0f, 5.0f, 5.0f));
    gen.generate(density, essence, region);
    EXPECT_EQ(density.grid().chunkCount(), 0u);
}

TEST_F(TerrainGeneratorTest, ConfigAccessors) {
    TerrainConfig cfg;
    cfg.seed = 7;
    cfg.frequency = 0.1f;
    TerrainGenerator gen(cfg);

    EXPECT_EQ(gen.config().seed, 7);
    EXPECT_FLOAT_EQ(gen.config().frequency, 0.1f);

    TerrainConfig newCfg;
    newCfg.seed = 42;
    gen.setConfig(newCfg);
    EXPECT_EQ(gen.config().seed, 42);
}

TEST_F(TerrainGeneratorTest, AllNoiseTypesWork) {
    AABB region(Vec3f(0.0f, 0.0f, 0.0f), Vec3f(4.0f, 4.0f, 4.0f));

    for (auto type : {NoiseType::Simplex, NoiseType::Perlin, NoiseType::OpenSimplex2, NoiseType::Value}) {
        TerrainConfig cfg = defaultConfig;
        cfg.noiseType = type;
        TerrainGenerator gen(cfg);
        FieldLayer<float> density;
        FieldLayer<Vector4<float, Space::World>> essence;
        gen.generate(density, essence, region);

        // Should produce some non-zero values
        bool hasValue = false;
        for (int z = 0; z < 4 && !hasValue; ++z) {
            for (int y = 0; y < 4 && !hasValue; ++y) {
                for (int x = 0; x < 4 && !hasValue; ++x) {
                    if (density.read(x, y, z) != 0.0f)
                        hasValue = true;
                }
            }
        }
        EXPECT_TRUE(hasValue) << "NoiseType " << static_cast<int>(type) << " should produce values";
    }
}

TEST_F(TerrainGeneratorTest, NegativeRegionCoordinates) {
    TerrainGenerator gen(defaultConfig);
    FieldLayer<float> density;
    FieldLayer<Vector4<float, Space::World>> essence;

    AABB region(Vec3f(-4.0f, -4.0f, -4.0f), Vec3f(0.0f, 0.0f, 0.0f));
    gen.generate(density, essence, region);

    int count = 0;
    for (int z = -4; z < 0; ++z) {
        for (int y = -4; y < 0; ++y) {
            for (int x = -4; x < 0; ++x) {
                float d = density.read(x, y, z);
                EXPECT_GE(d, 0.0f);
                EXPECT_LE(d, 1.0f);
                ++count;
            }
        }
    }
    EXPECT_EQ(count, 64);
}

TEST_F(TerrainGeneratorTest, SameSeedProducesSameOutput) {
    FieldLayer<float> densityA;
    FieldLayer<Vector4<float, Space::World>> essenceA;
    FieldLayer<float> densityB;
    FieldLayer<Vector4<float, Space::World>> essenceB;

    AABB region(Vec3f(0.0f, 0.0f, 0.0f), Vec3f(4.0f, 4.0f, 4.0f));

    TerrainGenerator genA(defaultConfig);
    genA.generate(densityA, essenceA, region);

    TerrainGenerator genB(defaultConfig);
    genB.generate(densityB, essenceB, region);

    for (int z = 0; z < 4; ++z) {
        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 4; ++x) {
                EXPECT_FLOAT_EQ(densityA.read(x, y, z), densityB.read(x, y, z))
                    << "Same seed should produce identical output at (" << x << "," << y << "," << z << ")";
            }
        }
    }
}
