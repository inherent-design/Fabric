#include "recurse/world/SnapMCMesher.hh"

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

auto planeDensity = [](int /*wx*/, int wy, int /*wz*/) -> float {
    return 17.0f - static_cast<float>(wy);
};

} // namespace

class SnapMCMesherTest : public ::testing::Test {
  protected:
    ChunkDensityCache density;
    ChunkMaterialCache material;
};

TEST_F(SnapMCMesherTest, EmptyCacheProducesNoMesh) {
    fillCache(density, [](int, int, int) { return 0.0f; });
    fillMaterialCache(material, [](int, int, int) -> uint16_t { return 0; });

    SnapMCMesher mesher;
    auto mesh = mesher.meshChunk(density, material, 0.5f, 0);
    EXPECT_TRUE(mesh.empty());
}

TEST_F(SnapMCMesherTest, FullSolidProducesNoMesh) {
    fillCache(density, [](int, int, int) { return 1.0f; });
    fillMaterialCache(material, [](int, int, int) -> uint16_t { return 1; });

    SnapMCMesher mesher;
    auto mesh = mesher.meshChunk(density, material, 0.5f, 0);
    EXPECT_TRUE(mesh.empty());
}

TEST_F(SnapMCMesherTest, SphereProducesClosedMesh) {
    fillCache(density, sphereDensity);
    fillMaterialCache(material, [](int, int, int) -> uint16_t { return 1; });

    SnapMCMesher mesher;
    auto mesh = mesher.meshChunk(density, material, 0.0f, 0);

    EXPECT_GT(mesh.vertices.size(), 0u);
    EXPECT_GT(mesh.indices.size(), 0u);
    EXPECT_EQ(mesh.indices.size() % 3, 0u);

    // No degenerate triangles (3 distinct indices per tri)
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        EXPECT_NE(mesh.indices[i], mesh.indices[i + 1]);
        EXPECT_NE(mesh.indices[i + 1], mesh.indices[i + 2]);
        EXPECT_NE(mesh.indices[i], mesh.indices[i + 2]);
    }
}

TEST_F(SnapMCMesherTest, FlatPlaneProducesFlatSurface) {
    fillCache(density, planeDensity);
    fillMaterialCache(material, [](int, int, int) -> uint16_t { return 1; });

    SnapMCMesher mesher;
    auto mesh = mesher.meshChunk(density, material, 0.0f, 0);

    EXPECT_GT(mesh.vertices.size(), 0u);

    for (const auto& v : mesh.vertices) {
        EXPECT_NEAR(v.py, 17.0f, 0.5f);
        // Normal should point approximately up (+Y)
        EXPECT_GT(v.ny, 0.5f);
    }
}

TEST_F(SnapMCMesherTest, TriangleCountLessThanStandardMC) {
    fillCache(density, sphereDensity);
    fillMaterialCache(material, [](int, int, int) -> uint16_t { return 1; });

    // Standard MC (no snapping)
    SnapMCMesher::Config noSnap;
    noSnap.snapEpsilon = 0.0f;
    SnapMCMesher standardMC(noSnap);
    auto meshStandard = standardMC.meshChunk(density, material, 0.0f, 0);

    // SnapMC with default epsilon
    SnapMCMesher snapMC;
    auto meshSnap = snapMC.meshChunk(density, material, 0.0f, 0);

    // SnapMC should produce <= triangles than standard MC
    EXPECT_LE(meshSnap.indices.size(), meshStandard.indices.size());
}

TEST_F(SnapMCMesherTest, LOD1ReducesCellCount) {
    fillCache(density, sphereDensity);
    fillMaterialCache(material, [](int, int, int) -> uint16_t { return 1; });

    SnapMCMesher mesher;
    auto meshLod0 = mesher.meshChunk(density, material, 0.0f, 0);
    auto meshLod1 = mesher.meshChunk(density, material, 0.0f, 1);

    EXPECT_GT(meshLod0.vertices.size(), 0u);
    EXPECT_GT(meshLod1.vertices.size(), 0u);
    EXPECT_LT(meshLod1.vertices.size(), meshLod0.vertices.size());
}

TEST_F(SnapMCMesherTest, LOD2ReducesCellCount) {
    fillCache(density, sphereDensity);
    fillMaterialCache(material, [](int, int, int) -> uint16_t { return 1; });

    SnapMCMesher mesher;
    auto meshLod0 = mesher.meshChunk(density, material, 0.0f, 0);
    auto meshLod2 = mesher.meshChunk(density, material, 0.0f, 2);

    EXPECT_GT(meshLod0.vertices.size(), 0u);
    EXPECT_GT(meshLod2.vertices.size(), 0u);
    EXPECT_LT(meshLod2.vertices.size(), meshLod0.vertices.size());
}

TEST_F(SnapMCMesherTest, VertexPositionsWithinChunkBounds) {
    fillCache(density, sphereDensity);
    fillMaterialCache(material, [](int, int, int) -> uint16_t { return 1; });

    SnapMCMesher mesher;
    auto mesh = mesher.meshChunk(density, material, 0.0f, 0);

    for (const auto& v : mesh.vertices) {
        EXPECT_GE(v.px, 0.0f);
        EXPECT_LE(v.px, 32.0f);
        EXPECT_GE(v.py, 0.0f);
        EXPECT_LE(v.py, 32.0f);
        EXPECT_GE(v.pz, 0.0f);
        EXPECT_LE(v.pz, 32.0f);
    }
}

TEST_F(SnapMCMesherTest, NormalsAreUnitLength) {
    fillCache(density, sphereDensity);
    fillMaterialCache(material, [](int, int, int) -> uint16_t { return 1; });

    SnapMCMesher mesher;
    auto mesh = mesher.meshChunk(density, material, 0.0f, 0);

    for (const auto& v : mesh.vertices) {
        float len = std::sqrt(v.nx * v.nx + v.ny * v.ny + v.nz * v.nz);
        EXPECT_NEAR(len, 1.0f, 0.01f);
    }
}

TEST_F(SnapMCMesherTest, MaterialsAreValid) {
    fillCache(density, sphereDensity);
    fillMaterialCache(material, [](int, int, int) -> uint16_t { return 42; });

    SnapMCMesher mesher;
    auto mesh = mesher.meshChunk(density, material, 0.0f, 0);

    EXPECT_GT(mesh.vertices.size(), 0u);
    for (const auto& v : mesh.vertices) {
        EXPECT_NE(v.material, 0u);
        EXPECT_EQ(v.getMaterialId(), 42);
    }
}
