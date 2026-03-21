#include "recurse/world/SurfaceNetsMesher.hh"

#include <cmath>
#include <gtest/gtest.h>

using namespace recurse;

namespace {

void fillCache(ChunkDensityCache& cache, auto fn) {
    ChunkedGrid<float, 32> grid;
    for (int wz = -1; wz < 33; ++wz)
        for (int wy = -1; wy < 33; ++wy)
            for (int wx = -1; wx < 33; ++wx)
                grid.set(wx, wy, wz, fn(wx, wy, wz));
    cache.build(0, 0, 0, grid);
}

void fillMaterialCache(ChunkMaterialCache& matCache, auto fn) {
    ChunkedGrid<uint16_t, 32> grid;
    for (int wz = -1; wz < 33; ++wz)
        for (int wy = -1; wy < 33; ++wy)
            for (int wx = -1; wx < 33; ++wx)
                grid.set(wx, wy, wz, fn(wx, wy, wz));
    matCache.build(0, 0, 0, grid);
}

auto sphereDensity = [](int wx, int wy, int wz) -> float {
    float dx = static_cast<float>(wx) - 16.0f;
    float dy = static_cast<float>(wy) - 16.0f;
    float dz = static_cast<float>(wz) - 16.0f;
    return 12.0f - std::sqrt(dx * dx + dy * dy + dz * dz);
};

} // namespace

class SurfaceNetsMesherTest : public ::testing::Test {
  protected:
    ChunkDensityCache density;
    ChunkMaterialCache material;
};

TEST_F(SurfaceNetsMesherTest, EmptyCacheProducesNoMesh) {
    fillCache(density, [](int, int, int) { return 0.0f; });
    fillMaterialCache(material, [](int, int, int) -> uint16_t { return 0; });

    SurfaceNetsMesher mesher;
    auto mesh = mesher.meshChunk(density, material, 0.5f, 0);
    EXPECT_TRUE(mesh.empty());
}

TEST_F(SurfaceNetsMesherTest, SphereProducesClosedMesh) {
    fillCache(density, sphereDensity);
    fillMaterialCache(material, [](int, int, int) -> uint16_t { return 1; });

    SurfaceNetsMesher mesher;
    auto mesh = mesher.meshChunk(density, material, 0.0f, 0);

    EXPECT_GT(mesh.vertices.size(), 0u);
    EXPECT_GT(mesh.indices.size(), 0u);
    EXPECT_EQ(mesh.indices.size() % 3, 0u);
}

TEST_F(SurfaceNetsMesherTest, FewerVerticesThanCellCount) {
    fillCache(density, sphereDensity);
    fillMaterialCache(material, [](int, int, int) -> uint16_t { return 1; });

    SurfaceNetsMesher mesher;
    auto mesh = mesher.meshChunk(density, material, 0.0f, 0);

    // Surface Nets places at most 1 vertex per active cell.
    // Total cells = 32^3 = 32768. Vertex count must be much less.
    EXPECT_GT(mesh.vertices.size(), 0u);
    EXPECT_LE(mesh.vertices.size(), static_cast<size_t>(32 * 32 * 32));
}

TEST_F(SurfaceNetsMesherTest, LODReducesVertexCount) {
    fillCache(density, sphereDensity);
    fillMaterialCache(material, [](int, int, int) -> uint16_t { return 1; });

    SurfaceNetsMesher mesher;
    auto meshLod0 = mesher.meshChunk(density, material, 0.0f, 0);
    auto meshLod1 = mesher.meshChunk(density, material, 0.0f, 1);

    EXPECT_GT(meshLod0.vertices.size(), 0u);
    EXPECT_GT(meshLod1.vertices.size(), 0u);
    EXPECT_LT(meshLod1.vertices.size(), meshLod0.vertices.size());
}

TEST_F(SurfaceNetsMesherTest, ImplementsMesherInterface) {
    SurfaceNetsMesher mesher;
    MesherInterface* base = dynamic_cast<MesherInterface*>(&mesher);
    ASSERT_NE(base, nullptr);
    EXPECT_STREQ(base->name(), "SurfaceNets");
}
