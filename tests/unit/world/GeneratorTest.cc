#include "fabric/simulation/SimulationGrid.hh"
#include "fabric/simulation/VoxelMaterial.hh"
#include "fabric/world/FlatGenerator.hh"
#include "fabric/world/LayeredGenerator.hh"
#include "fabric/world/SingleMaterialGenerator.hh"
#include <gtest/gtest.h>

using namespace fabric::simulation;
using namespace fabric::world;

class GeneratorTest : public ::testing::Test {
  protected:
    SimulationGrid grid;
};

// 1. FlatBelowSurface -- Below = Stone
TEST_F(GeneratorTest, FlatBelowSurface) {
    FlatGenerator gen(16);
    // Chunk at y=0 covers worldY 0-31, surface at 16
    gen.generate(grid, {0, 0, 0});
    grid.advanceEpoch();

    // worldY=0 should be Stone
    EXPECT_EQ(grid.readCell(0, 0, 0).materialId, MaterialIds::Stone);
    // worldY=15 should be Stone
    EXPECT_EQ(grid.readCell(0, 15, 0).materialId, MaterialIds::Stone);
}

// 2. FlatAboveSurface -- Above = Air
TEST_F(GeneratorTest, FlatAboveSurface) {
    FlatGenerator gen(16);
    gen.generate(grid, {0, 0, 0});
    grid.advanceEpoch();

    // worldY=17 should be Air
    EXPECT_EQ(grid.readCell(0, 17, 0).materialId, MaterialIds::Air);
    // worldY=31 should be Air
    EXPECT_EQ(grid.readCell(0, 31, 0).materialId, MaterialIds::Air);
}

// 3. FlatSurfaceLayer -- Surface = Dirt
TEST_F(GeneratorTest, FlatSurfaceLayer) {
    FlatGenerator gen(16);
    gen.generate(grid, {0, 0, 0});
    grid.advanceEpoch();

    EXPECT_EQ(grid.readCell(0, 16, 0).materialId, MaterialIds::Dirt);
    EXPECT_EQ(grid.readCell(15, 16, 15).materialId, MaterialIds::Dirt);
}

// 4. SingleMaterialFills -- All cells = specified material
TEST_F(GeneratorTest, SingleMaterialFills) {
    VoxelCell water;
    water.materialId = MaterialIds::Water;
    SingleMaterialGenerator gen(water);
    gen.generate(grid, {0, 0, 0});

    // SingleMaterial uses fillChunk (sentinel), so reads should return Water
    EXPECT_EQ(grid.readCell(0, 0, 0).materialId, MaterialIds::Water);
    EXPECT_EQ(grid.readCell(16, 16, 16).materialId, MaterialIds::Water);
    EXPECT_EQ(grid.readCell(31, 31, 31).materialId, MaterialIds::Water);

    // Should not be materialized (sentinel optimization)
    EXPECT_FALSE(grid.isChunkMaterialized(0, 0, 0));
}

// 5. LayeredBoundaries -- Exact Y boundaries
TEST_F(GeneratorTest, LayeredBoundaries) {
    VoxelCell stone;
    stone.materialId = MaterialIds::Stone;
    VoxelCell dirt;
    dirt.materialId = MaterialIds::Dirt;

    LayeredGenerator gen({{stone, 0, 9}, {dirt, 10, 19}});
    gen.generate(grid, {0, 0, 0});
    grid.advanceEpoch();

    EXPECT_EQ(grid.readCell(0, 9, 0).materialId, MaterialIds::Stone);
    EXPECT_EQ(grid.readCell(0, 10, 0).materialId, MaterialIds::Dirt);
    EXPECT_EQ(grid.readCell(0, 19, 0).materialId, MaterialIds::Dirt);
    // worldY=20 above both layers -> Air (default fill)
    EXPECT_EQ(grid.readCell(0, 20, 0).materialId, MaterialIds::Air);
}

// 6. LayeredMultiple -- 3+ layers correct
TEST_F(GeneratorTest, LayeredMultiple) {
    VoxelCell stone, dirt, sand;
    stone.materialId = MaterialIds::Stone;
    dirt.materialId = MaterialIds::Dirt;
    sand.materialId = MaterialIds::Sand;

    LayeredGenerator gen({{stone, 0, 4}, {dirt, 5, 9}, {sand, 10, 14}});
    gen.generate(grid, {0, 0, 0});
    grid.advanceEpoch();

    EXPECT_EQ(grid.readCell(0, 2, 0).materialId, MaterialIds::Stone);
    EXPECT_EQ(grid.readCell(0, 7, 0).materialId, MaterialIds::Dirt);
    EXPECT_EQ(grid.readCell(0, 12, 0).materialId, MaterialIds::Sand);
}

// 7. NoBgfxDependency -- Compiles without bgfx (verified by this test compiling)
TEST_F(GeneratorTest, NoBgfxDependency) {
    // If this test compiles and links, generators have no bgfx dependency.
    FlatGenerator flat(16);
    SingleMaterialGenerator single(VoxelCell{});
    LayeredGenerator layered({});
    EXPECT_EQ(flat.name(), "Flat");
    EXPECT_EQ(single.name(), "SingleMaterial");
    EXPECT_EQ(layered.name(), "Layered");
}

// 8. NegativeYChunks -- Below ground = Stone (test chunk at cy=-1)
TEST_F(GeneratorTest, NegativeYChunks) {
    FlatGenerator gen(16);
    // Chunk at cy=-1 covers worldY -32 to -1, all below surface
    gen.generate(grid, {0, -1, 0});
    grid.advanceEpoch();

    EXPECT_EQ(grid.readCell(0, -1, 0).materialId, MaterialIds::Stone);
    EXPECT_EQ(grid.readCell(0, -32, 0).materialId, MaterialIds::Stone);
}

// 9. AboveSurfaceAllAir -- chunk above surface = all air (sentinel)
TEST_F(GeneratorTest, AboveSurfaceAllAir) {
    FlatGenerator gen(16);
    // Chunk at cy=1 covers worldY 32-63, entirely above surface
    gen.generate(grid, {0, 1, 0});

    // Should remain sentinel (not materialized)
    EXPECT_FALSE(grid.isChunkMaterialized(0, 1, 0));

    // All reads should return Air
    EXPECT_EQ(grid.readCell(0, 32, 0).materialId, MaterialIds::Air);
    EXPECT_EQ(grid.readCell(0, 63, 0).materialId, MaterialIds::Air);
}

// 10. GeneratorName -- Verify name() returns correct string
TEST_F(GeneratorTest, GeneratorName) {
    FlatGenerator flat(16);
    SingleMaterialGenerator single(VoxelCell{});
    LayeredGenerator layered({});

    EXPECT_EQ(flat.name(), "Flat");
    EXPECT_EQ(single.name(), "SingleMaterial");
    EXPECT_EQ(layered.name(), "Layered");
}
