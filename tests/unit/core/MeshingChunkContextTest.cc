#include "recurse/systems/VoxelMeshingSystem.hh"

#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelMaterial.hh"

#include <gtest/gtest.h>

using recurse::simulation::K_CHUNK_SIZE;
using recurse::simulation::K_CHUNK_VOLUME;
using recurse::simulation::SimulationGrid;
using recurse::simulation::VoxelCell;
using recurse::systems::MeshingChunkContext;
namespace MaterialIds = recurse::simulation::material_ids;

class MeshingChunkContextTest : public ::testing::Test {
  protected:
    SimulationGrid simGrid;

    void SetUp() override {
        // Center chunk (0,0,0) filled with STONE
        simGrid.materializeChunk(0, 0, 0);
        fillAll(0, 0, 0, MaterialIds::STONE);

        // +X neighbor (1,0,0) filled with DIRT
        simGrid.materializeChunk(1, 0, 0);
        fillAll(1, 0, 0, MaterialIds::DIRT);

        // -X neighbor (-1,0,0) filled with SAND
        simGrid.materializeChunk(-1, 0, 0);
        fillAll(-1, 0, 0, MaterialIds::SAND);

        // +Y neighbor (0,1,0) filled with WATER
        simGrid.materializeChunk(0, 1, 0);
        fillAll(0, 1, 0, MaterialIds::WATER);

        // -Y neighbor (0,-1,0) filled with GRAVEL
        simGrid.materializeChunk(0, -1, 0);
        fillAll(0, -1, 0, MaterialIds::GRAVEL);

        // +Z neighbor (0,0,1) filled with DIRT
        simGrid.materializeChunk(0, 0, 1);
        fillAll(0, 0, 1, MaterialIds::DIRT);

        // -Z neighbor (0,0,-1) filled with WATER
        simGrid.materializeChunk(0, 0, -1);
        fillAll(0, 0, -1, MaterialIds::WATER);

        simGrid.syncChunkBuffers(0, 0, 0);
        simGrid.syncChunkBuffers(1, 0, 0);
        simGrid.syncChunkBuffers(-1, 0, 0);
        simGrid.syncChunkBuffers(0, 1, 0);
        simGrid.syncChunkBuffers(0, -1, 0);
        simGrid.syncChunkBuffers(0, 0, 1);
        simGrid.syncChunkBuffers(0, 0, -1);
    }

    void fillAll(int cx, int cy, int cz, uint16_t materialId) {
        VoxelCell cell{materialId};
        int bx = cx * K_CHUNK_SIZE;
        int by = cy * K_CHUNK_SIZE;
        int bz = cz * K_CHUNK_SIZE;
        for (int lz = 0; lz < K_CHUNK_SIZE; ++lz)
            for (int ly = 0; ly < K_CHUNK_SIZE; ++ly)
                for (int lx = 0; lx < K_CHUNK_SIZE; ++lx)
                    simGrid.writeCell(bx + lx, by + ly, bz + lz, cell);
    }

    void writeSingleCell(int cx, int cy, int cz, int lx, int ly, int lz, uint16_t materialId) {
        int wx = cx * K_CHUNK_SIZE + lx;
        int wy = cy * K_CHUNK_SIZE + ly;
        int wz = cz * K_CHUNK_SIZE + lz;
        simGrid.writeCell(wx, wy, wz, VoxelCell{materialId});
    }

    MeshingChunkContext buildCtx() {
        MeshingChunkContext ctx{};
        ctx.cx = 0;
        ctx.cy = 0;
        ctx.cz = 0;
        ctx.self = simGrid.readBuffer(0, 0, 0);
        ctx.selfFill = simGrid.getChunkFillValue(0, 0, 0);

        static constexpr int K_OFFSETS[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
        for (int i = 0; i < 6; ++i) {
            int nx = K_OFFSETS[i][0];
            int ny = K_OFFSETS[i][1];
            int nz = K_OFFSETS[i][2];
            ctx.neighbors[i] = simGrid.readBuffer(nx, ny, nz);
            ctx.neighborFill[i] = simGrid.getChunkFillValue(nx, ny, nz);
        }
        return ctx;
    }
};

TEST_F(MeshingChunkContextTest, ReadLocalInBounds) {
    auto ctx = buildCtx();
    auto cell = ctx.readLocal(0, 0, 0, &simGrid);
    EXPECT_EQ(cell.materialId, MaterialIds::STONE);

    auto mid = ctx.readLocal(K_CHUNK_SIZE / 2, K_CHUNK_SIZE / 2, K_CHUNK_SIZE / 2, &simGrid);
    EXPECT_EQ(mid.materialId, MaterialIds::STONE);

    auto edge = ctx.readLocal(K_CHUNK_SIZE - 1, K_CHUNK_SIZE - 1, K_CHUNK_SIZE - 1, &simGrid);
    EXPECT_EQ(edge.materialId, MaterialIds::STONE);
}

TEST_F(MeshingChunkContextTest, ReadLocalFaceNeighborPlusX) {
    auto ctx = buildCtx();
    auto cell = ctx.readLocal(K_CHUNK_SIZE, 0, 0, &simGrid);
    EXPECT_EQ(cell.materialId, MaterialIds::DIRT);
}

TEST_F(MeshingChunkContextTest, ReadLocalFaceNeighborMinusX) {
    auto ctx = buildCtx();
    auto cell = ctx.readLocal(-1, 0, 0, &simGrid);
    EXPECT_EQ(cell.materialId, MaterialIds::SAND);
}

TEST_F(MeshingChunkContextTest, ReadLocalFaceNeighborPlusY) {
    auto ctx = buildCtx();
    auto cell = ctx.readLocal(0, K_CHUNK_SIZE, 0, &simGrid);
    EXPECT_EQ(cell.materialId, MaterialIds::WATER);
}

TEST_F(MeshingChunkContextTest, ReadLocalFaceNeighborMinusZ) {
    auto ctx = buildCtx();
    auto cell = ctx.readLocal(0, 0, -1, &simGrid);
    EXPECT_EQ(cell.materialId, MaterialIds::WATER);
}

TEST_F(MeshingChunkContextTest, ReadLocalNullNeighborReturnsFill) {
    auto ctx = buildCtx();

    // Null out the +X neighbor (face index 0) and set its fill to GRAVEL
    ctx.neighbors[0] = nullptr;
    ctx.neighborFill[0] = VoxelCell{MaterialIds::GRAVEL};

    auto cell = ctx.readLocal(K_CHUNK_SIZE, 5, 5, &simGrid);
    EXPECT_EQ(cell.materialId, MaterialIds::GRAVEL);
}

TEST_F(MeshingChunkContextTest, ReadLocalEdgeCellFallback) {
    // 2 axes out of bounds: lx=K_CHUNK_SIZE, ly=K_CHUNK_SIZE (edge cell)
    // Falls through to the simGrid.readCell fallback path.
    // The diagonal chunk (1,1,0) is not materialized, so readCell returns default VoxelCell{AIR}.
    auto ctx = buildCtx();
    auto cell = ctx.readLocal(K_CHUNK_SIZE, K_CHUNK_SIZE, 0, &simGrid);
    EXPECT_EQ(cell.materialId, MaterialIds::AIR);

    // Materialize the diagonal chunk with SAND and verify fallback resolves it
    simGrid.materializeChunk(1, 1, 0);
    fillAll(1, 1, 0, MaterialIds::SAND);
    simGrid.syncChunkBuffers(1, 1, 0);

    cell = ctx.readLocal(K_CHUNK_SIZE, K_CHUNK_SIZE, 0, &simGrid);
    EXPECT_EQ(cell.materialId, MaterialIds::SAND);
}

TEST_F(MeshingChunkContextTest, ReadLocalEdgeCellNullFallback) {
    auto ctx = buildCtx();
    // Corner cell: all 3 axes out of bounds, null fallback
    auto cell = ctx.readLocal(K_CHUNK_SIZE, K_CHUNK_SIZE, K_CHUNK_SIZE, static_cast<const SimulationGrid*>(nullptr));
    EXPECT_EQ(cell.materialId, MaterialIds::AIR);
}

TEST_F(MeshingChunkContextTest, BuildMeshingContextResolvesNeighbors) {
    auto ctx = buildCtx();

    for (int i = 0; i < 6; ++i)
        EXPECT_NE(ctx.neighbors[i], nullptr) << "face " << i << " should be non-null";

    // Remove the +Z neighbor and rebuild
    simGrid.removeChunk(0, 0, 1);
    auto ctx2 = buildCtx();
    EXPECT_EQ(ctx2.neighbors[4], nullptr);

    // Other 5 neighbors should still resolve
    EXPECT_NE(ctx2.neighbors[0], nullptr);
    EXPECT_NE(ctx2.neighbors[1], nullptr);
    EXPECT_NE(ctx2.neighbors[2], nullptr);
    EXPECT_NE(ctx2.neighbors[3], nullptr);
    EXPECT_NE(ctx2.neighbors[5], nullptr);
}
