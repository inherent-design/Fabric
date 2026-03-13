#include "recurse/simulation/GhostCells.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelMaterial.hh"
#include <gtest/gtest.h>

using namespace recurse::simulation;
using fabric::K_CHUNK_SIZE;

class GhostCellsTest : public ::testing::Test {
  protected:
    SimulationGrid grid;
    GhostCellManager ghosts;

    void writeAndAdvance(int wx, int wy, int wz, VoxelCell cell) {
        grid.writeCell(wx, wy, wz, cell);
        grid.advanceEpoch();
    }

    VoxelCell makeSand() {
        VoxelCell c;
        c.materialId = material_ids::SAND;
        return c;
    }

    VoxelCell makeStone() {
        VoxelCell c;
        c.materialId = material_ids::STONE;
        return c;
    }

    VoxelCell makeWater() {
        VoxelCell c;
        c.materialId = material_ids::WATER;
        return c;
    }
};

// 1. Ghost[+X][0][0] copies neighbor's local (0,0,0)
TEST_F(GhostCellsTest, SyncCopiesNeighborBoundary) {
    ChunkCoord origin{0, 0, 0};
    ChunkCoord neighbor{1, 0, 0}; // +X neighbor

    grid.fillChunk(0, 0, 0, VoxelCell{});
    grid.fillChunk(1, 0, 0, VoxelCell{});

    // Write sand at neighbor's local (0, 0, 0) = world (32, 0, 0)
    grid.writeCell(32, 0, 0, makeSand());
    grid.advanceEpoch();

    ghosts.syncGhostCells(origin, grid);

    // +X face, u=ly=0, v=lz=0 should be Sand
    VoxelCell ghost = ghosts.getStore(origin).get(Face::PosX, 0, 0);
    EXPECT_EQ(ghost.materialId, material_ids::SAND);
}

// 2. Ghost reads from read buffer, not write buffer
TEST_F(GhostCellsTest, GhostReflectsReadBuffer) {
    ChunkCoord origin{0, 0, 0};

    grid.fillChunk(0, 0, 0, VoxelCell{});
    grid.fillChunk(1, 0, 0, VoxelCell{});

    // Write stone to neighbor, advance so it's in read buffer
    grid.writeCell(32, 0, 0, makeStone());
    grid.advanceEpoch();

    // Write sand to same cell in write buffer (not yet advanced)
    grid.writeCell(32, 0, 0, makeSand());

    ghosts.syncGhostCells(origin, grid);

    // Should see Stone (read buffer), not Sand (write buffer)
    VoxelCell ghost = ghosts.getStore(origin).get(Face::PosX, 0, 0);
    EXPECT_EQ(ghost.materialId, material_ids::STONE);
}

// 3. Missing neighbor returns Air
TEST_F(GhostCellsTest, MissingNeighborReturnsAir) {
    ChunkCoord origin{0, 0, 0};
    grid.fillChunk(0, 0, 0, VoxelCell{});
    // No +X neighbor loaded

    ghosts.syncGhostCells(origin, grid);

    VoxelCell ghost = ghosts.getStore(origin).get(Face::PosX, 5, 5);
    EXPECT_EQ(ghost.materialId, material_ids::AIR);
}

// 4. All 6 faces copy correct neighbor slices
TEST_F(GhostCellsTest, AllSixFacesCorrect) {
    ChunkCoord origin{0, 0, 0};

    // Fill all 7 chunks
    grid.fillChunk(0, 0, 0, VoxelCell{});

    // Each neighbor gets a unique material at its boundary cell
    // +X: neighbor(1,0,0) local(0,5,5) = world(32,5,5)
    grid.fillChunk(1, 0, 0, VoxelCell{});
    VoxelCell c1;
    c1.materialId = material_ids::SAND;
    grid.writeCell(32, 5, 5, c1);

    // -X: neighbor(-1,0,0) local(31,5,5) = world(-1,5,5)
    grid.fillChunk(-1, 0, 0, VoxelCell{});
    VoxelCell c2;
    c2.materialId = material_ids::STONE;
    grid.writeCell(-1, 5, 5, c2);

    // +Y: neighbor(0,1,0) local(5,0,5) = world(5,32,5)
    grid.fillChunk(0, 1, 0, VoxelCell{});
    VoxelCell c3;
    c3.materialId = material_ids::DIRT;
    grid.writeCell(5, 32, 5, c3);

    // -Y: neighbor(0,-1,0) local(5,31,5) = world(5,-1,5)
    grid.fillChunk(0, -1, 0, VoxelCell{});
    VoxelCell c4;
    c4.materialId = material_ids::WATER;
    grid.writeCell(5, -1, 5, c4);

    // +Z: neighbor(0,0,1) local(5,5,0) = world(5,5,32)
    grid.fillChunk(0, 0, 1, VoxelCell{});
    VoxelCell c5;
    c5.materialId = material_ids::GRAVEL;
    grid.writeCell(5, 5, 32, c5);

    // -Z: neighbor(0,0,-1) local(5,5,31) = world(5,5,-1)
    grid.fillChunk(0, 0, -1, VoxelCell{});
    VoxelCell c6;
    c6.materialId = material_ids::SAND;
    c6.flags = voxel_flags::FREE_FALL; // distinguish from +X sand
    grid.writeCell(5, 5, -1, c6);

    grid.advanceEpoch();
    ghosts.syncGhostCells(origin, grid);

    auto& store = ghosts.getStore(origin);
    EXPECT_EQ(store.get(Face::PosX, 5, 5).materialId, material_ids::SAND);
    EXPECT_EQ(store.get(Face::NegX, 5, 5).materialId, material_ids::STONE);
    EXPECT_EQ(store.get(Face::PosY, 5, 5).materialId, material_ids::DIRT);
    EXPECT_EQ(store.get(Face::NegY, 5, 5).materialId, material_ids::WATER);
    EXPECT_EQ(store.get(Face::PosZ, 5, 5).materialId, material_ids::GRAVEL);
    EXPECT_EQ(store.get(Face::NegZ, 5, 5).materialId, material_ids::SAND);
    EXPECT_EQ(store.get(Face::NegZ, 5, 5).flags, voxel_flags::FREE_FALL);
}

// 5. Resync after modification picks up new values
TEST_F(GhostCellsTest, ResyncUpdatesValues) {
    ChunkCoord origin{0, 0, 0};

    grid.fillChunk(0, 0, 0, VoxelCell{});
    grid.fillChunk(1, 0, 0, VoxelCell{});

    // Initial: stone at neighbor boundary
    grid.writeCell(32, 0, 0, makeStone());
    grid.advanceEpoch();
    ghosts.syncGhostCells(origin, grid);
    EXPECT_EQ(ghosts.getStore(origin).get(Face::PosX, 0, 0).materialId, material_ids::STONE);

    // Update: sand at same position
    grid.writeCell(32, 0, 0, makeSand());
    grid.advanceEpoch();
    ghosts.syncGhostCells(origin, grid);
    EXPECT_EQ(ghosts.getStore(origin).get(Face::PosX, 0, 0).materialId, material_ids::SAND);
}

// 6. Ghost cell count: 6 * 1024 = 6144
TEST_F(GhostCellsTest, GhostCellCount) {
    constexpr int K_EXPECTED = K_FACE_COUNT * K_FACE_AREA;
    EXPECT_EQ(K_EXPECTED, 6144);

    // Verify storage size
    GhostCellStore store{};
    EXPECT_EQ(store.faces.size(), 6u);
    EXPECT_EQ(store.faces[0].size(), static_cast<size_t>(K_FACE_AREA));
}

// 7. readGhost maps out-of-bounds correctly
TEST_F(GhostCellsTest, ReadGhostMapsOutOfBounds) {
    ChunkCoord origin{0, 0, 0};

    grid.fillChunk(0, 0, 0, VoxelCell{});
    grid.fillChunk(-1, 0, 0, VoxelCell{});
    grid.fillChunk(1, 0, 0, VoxelCell{});

    // Put sand at neighbor(-1,0,0) local(31,5,5) = world(-1,5,5)
    grid.writeCell(-1, 5, 5, makeSand());
    // Put stone at neighbor(1,0,0) local(0,5,5) = world(32,5,5)
    grid.writeCell(32, 5, 5, makeStone());
    grid.advanceEpoch();

    ghosts.syncGhostCells(origin, grid);

    // lx=-1 -> NegX face, u=ly=5, v=lz=5
    VoxelCell negX = ghosts.readGhost(origin, -1, 5, 5);
    EXPECT_EQ(negX.materialId, material_ids::SAND);

    // lx=32 -> PosX face, u=ly=5, v=lz=5
    VoxelCell posX = ghosts.readGhost(origin, 32, 5, 5);
    EXPECT_EQ(posX.materialId, material_ids::STONE);
}

// 8. Negative chunk coordinates work correctly
TEST_F(GhostCellsTest, NegativeChunkCoordinates) {
    ChunkCoord origin{-1, -1, -1};
    ChunkCoord neighborPosX{0, -1, -1};

    grid.fillChunk(-1, -1, -1, VoxelCell{});
    grid.fillChunk(0, -1, -1, VoxelCell{});

    // Place sand at neighbor's local(0,0,0) = world(0,-32,-32)
    grid.writeCell(0, -32, -32, makeSand());
    grid.advanceEpoch();

    ghosts.syncGhostCells(origin, grid);

    VoxelCell ghost = ghosts.getStore(origin).get(Face::PosX, 0, 0);
    EXPECT_EQ(ghost.materialId, material_ids::SAND);

    // Also check via readGhost
    VoxelCell readG = ghosts.readGhost(origin, 32, 0, 0);
    EXPECT_EQ(readG.materialId, material_ids::SAND);
}
