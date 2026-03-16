#include "fabric/world/ChunkedGrid.hh"
#include "recurse/simulation/ChunkActivityTracker.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelMaterial.hh"
#include "recurse/systems/VoxelMeshingSystem.hh"
#include "recurse/world/ChunkDensityCache.hh"
#include "recurse/world/SnapMCMesher.hh"

#include <cmath>
#include <gtest/gtest.h>

using fabric::ChunkCoord;
using recurse::simulation::ChunkActivityTracker;
using recurse::simulation::ChunkState;
using recurse::simulation::K_CHUNK_SIZE;
using recurse::simulation::SimulationGrid;
using recurse::simulation::VoxelCell;
namespace MaterialIds = recurse::simulation::material_ids;
using recurse::systems::VoxelMeshingSystem;

// =============================================================================
// SimulationGrid Cross-Chunk Access Tests
// =============================================================================

class SimulationGridCrossChunkTest : public ::testing::Test {
  protected:
    SimulationGrid grid;

    void fillChunkWithSolid(int cx, int cy, int cz, uint16_t materialId = recurse::simulation::material_ids::STONE) {
        VoxelCell cell{materialId, 0, 0};
        grid.fillChunk(cx, cy, cz, cell);
    }
};

TEST_F(SimulationGridCrossChunkTest, ReadCellReturnsAirForMissingChunk) {
    // Reading from a chunk that was never added should return air
    VoxelCell cell = grid.readCell(0, 0, 0);
    EXPECT_EQ(cell.materialId, recurse::simulation::material_ids::AIR);
}

TEST_F(SimulationGridCrossChunkTest, ReadCellReturnsCorrectValueForExistingChunk) {
    fillChunkWithSolid(0, 0, 0, recurse::simulation::material_ids::STONE);

    VoxelCell cell = grid.readCell(0, 0, 0);
    EXPECT_EQ(cell.materialId, recurse::simulation::material_ids::STONE);
}

TEST_F(SimulationGridCrossChunkTest, ReadCellCrossesChunkBoundary) {
    // Fill chunk (0,0,0) with stone
    fillChunkWithSolid(0, 0, 0, recurse::simulation::material_ids::STONE);

    // Reading at local position (31,0,0) in chunk (0,0,0)
    // This is one voxel away from chunk (1,0,0)
    EXPECT_EQ(grid.readCell(31, 0, 0).materialId, recurse::simulation::material_ids::STONE);

    // Reading at (32,0,0) is in chunk (1,0,0) which doesn't exist -> should return air
    EXPECT_EQ(grid.readCell(32, 0, 0).materialId, recurse::simulation::material_ids::AIR);
}

TEST_F(SimulationGridCrossChunkTest, HasChunkReturnsCorrectStatus) {
    EXPECT_FALSE(grid.hasChunk(0, 0, 0));

    fillChunkWithSolid(0, 0, 0);
    EXPECT_TRUE(grid.hasChunk(0, 0, 0));

    EXPECT_FALSE(grid.hasChunk(1, 0, 0));
}

TEST_F(SimulationGridCrossChunkTest, AdjacentChunkCoordinates) {
    // Verify chunk coordinate calculations at boundaries
    // Chunk (0,0,0) spans world coords [0,31] in each axis
    fillChunkWithSolid(0, 0, 0);
    fillChunkWithSolid(1, 0, 0);

    // Local 31 in chunk 0, local 0 in chunk 1
    EXPECT_EQ(grid.readCell(31, 0, 0).materialId, recurse::simulation::material_ids::STONE);
    EXPECT_EQ(grid.readCell(32, 0, 0).materialId, recurse::simulation::material_ids::STONE);
}

// =============================================================================
// Cross-Chunk Mesh Boundary Tests
// =============================================================================

class CrossChunkMeshBoundaryTest : public ::testing::Test {
  protected:
    SimulationGrid simGrid;
    ChunkActivityTracker tracker;
    VoxelMeshingSystem meshingSystem;

    void SetUp() override {
        meshingSystem.setSimulationGrid(&simGrid);
        meshingSystem.setActivityTracker(&tracker);
        tracker.setReferencePoint(0, 0, 0);
        // Cross-chunk boundary tests need to mesh without full neighbor sets.
        meshingSystem.setRequireNeighborsForMeshing(false);
    }

    void fillChunkRegion(int cx, int cy, int cz, uint16_t materialId = recurse::simulation::material_ids::STONE) {
        VoxelCell cell{materialId, 0, 0};
        simGrid.fillChunk(cx, cy, cz, cell);
    }

    void writeSolidBox(int cx, int cy, int cz) {
        // Write actual voxels to the chunk
        int baseX = cx * K_CHUNK_SIZE;
        int baseY = cy * K_CHUNK_SIZE;
        int baseZ = cz * K_CHUNK_SIZE;

        VoxelCell solid{recurse::simulation::material_ids::STONE, 0, 0};
        for (int z = 0; z < K_CHUNK_SIZE; ++z) {
            for (int y = 0; y < K_CHUNK_SIZE; ++y) {
                for (int x = 0; x < K_CHUNK_SIZE; ++x) {
                    simGrid.writeCell(baseX + x, baseY + y, baseZ + z, solid);
                }
            }
        }
        simGrid.advanceEpoch();
    }
};

TEST_F(CrossChunkMeshBoundaryTest, SingleChunkProducesMesh) {
    // Basic sanity check: a solid chunk should produce a mesh
    fillChunkRegion(0, 0, 0);
    tracker.setState({0, 0, 0}, ChunkState::Active);

    meshingSystem.processFrame();

    const auto& meshes = meshingSystem.gpuMeshes();
    ASSERT_EQ(meshes.count(ChunkCoord{0, 0, 0}), 1u);
    EXPECT_TRUE(meshes.at(ChunkCoord{0, 0, 0}).valid);
}

TEST_F(CrossChunkMeshBoundaryTest, ChunkMeshedWithMissingNeighborHasBoundaryVertices) {
    // When chunk (0,0,0) is meshed but chunk (1,0,0) doesn't exist,
    // the mesh should still be produced (blur handles missing neighbors)

    writeSolidBox(0, 0, 0);
    tracker.setState({0, 0, 0}, ChunkState::Active);

    meshingSystem.processFrame();

    const auto& meshes = meshingSystem.gpuMeshes();
    ASSERT_EQ(meshes.count(ChunkCoord{0, 0, 0}), 1u);
    EXPECT_TRUE(meshes.at(ChunkCoord{0, 0, 0}).valid);

    // The mesh should have vertices (solid chunk with air around it)
    EXPECT_GT(meshes.at(ChunkCoord{0, 0, 0}).vertexCount, 0u);
}

TEST_F(CrossChunkMeshBoundaryTest, LoadingAdjacentChunkTriggersRemesh_IfNotified) {
    // This test documents the EXPECTED behavior:
    // When chunk B loads adjacent to existing chunk A,
    // chunk A should be notified to remesh its boundary

    // Step 1: Mesh chunk (0,0,0) when (1,0,0) doesn't exist
    writeSolidBox(0, 0, 0);
    tracker.setState({0, 0, 0}, ChunkState::Active);
    meshingSystem.processFrame();

    const auto& meshes = meshingSystem.gpuMeshes();
    ASSERT_EQ(meshes.count(ChunkCoord{0, 0, 0}), 1u);
    size_t firstVertexCount = meshes.at(ChunkCoord{0, 0, 0}).vertexCount;

    // Step 2: Load chunk (1,0,0) and NOTIFY chunk (0,0,0)
    writeSolidBox(1, 0, 0);
    tracker.setState({1, 0, 0}, ChunkState::Active);

    // THIS IS THE KEY: notify chunk (0,0,0) that its neighbor changed
    // This is what SHOULD happen but currently DOESN'T
    tracker.notifyBoundaryChange({0, 0, 0});

    meshingSystem.processFrame();

    // Both chunks should now be meshed
    EXPECT_EQ(meshes.count(ChunkCoord{0, 0, 0}), 1u);
    EXPECT_EQ(meshes.count(ChunkCoord{1, 0, 0}), 1u);

    // The vertex count of chunk (0,0,0) may have changed because
    // the boundary face between (0,0,0) and (1,0,0) is now occluded
    // (This is a behavioral test - exact count depends on meshing algorithm)
}

TEST_F(CrossChunkMeshBoundaryTest, MeshBoundaryDiffersWithAndWithoutNeighbor) {
    // Verify that a chunk meshed with an adjacent solid neighbor
    // produces different geometry than when meshed alone

    // Case A: Chunk (0,0,0) meshed alone (no neighbor at +X)
    SimulationGrid gridA;
    ChunkActivityTracker trackerA;
    VoxelMeshingSystem systemA;
    systemA.setSimulationGrid(&gridA);
    systemA.setActivityTracker(&trackerA);
    systemA.setRequireNeighborsForMeshing(false); // Mesh without full neighbor set
    trackerA.setReferencePoint(0, 0, 0);

    VoxelCell solid{recurse::simulation::material_ids::STONE, 0, 0};
    for (int z = 0; z < K_CHUNK_SIZE; ++z) {
        for (int y = 0; y < K_CHUNK_SIZE; ++y) {
            for (int x = 0; x < K_CHUNK_SIZE; ++x) {
                gridA.writeCell(x, y, z, solid);
            }
        }
    }
    gridA.advanceEpoch();
    trackerA.setState({0, 0, 0}, ChunkState::Active);
    systemA.processFrame();

    size_t aloneVertexCount = systemA.gpuMeshes().at(ChunkCoord{0, 0, 0}).vertexCount;

    // Case B: Chunk (0,0,0) meshed WITH neighbor at +X
    SimulationGrid gridB;
    ChunkActivityTracker trackerB;
    VoxelMeshingSystem systemB;
    systemB.setSimulationGrid(&gridB);
    systemB.setActivityTracker(&trackerB);
    systemB.setRequireNeighborsForMeshing(false); // Mesh without full neighbor set
    trackerB.setReferencePoint(0, 0, 0);

    // Fill chunk (0,0,0)
    for (int z = 0; z < K_CHUNK_SIZE; ++z) {
        for (int y = 0; y < K_CHUNK_SIZE; ++y) {
            for (int x = 0; x < K_CHUNK_SIZE; ++x) {
                gridB.writeCell(x, y, z, solid);
            }
        }
    }
    // Fill chunk (1,0,0) - the adjacent chunk
    for (int z = 0; z < K_CHUNK_SIZE; ++z) {
        for (int y = 0; y < K_CHUNK_SIZE; ++y) {
            for (int x = 0; x < K_CHUNK_SIZE; ++x) {
                gridB.writeCell(K_CHUNK_SIZE + x, y, z, solid);
            }
        }
    }
    gridB.advanceEpoch();
    trackerB.setState({0, 0, 0}, ChunkState::Active);
    trackerB.setState({1, 0, 0}, ChunkState::Active);
    systemB.processFrame();

    size_t withNeighborVertexCount = systemB.gpuMeshes().at(ChunkCoord{0, 0, 0}).vertexCount;

    // When the +X face is against solid instead of air, fewer surface vertices are needed
    // (the face between chunks becomes interior, not surface)
    EXPECT_LT(withNeighborVertexCount, aloneVertexCount)
        << "Chunk with solid neighbor should have fewer vertices (occluded face) "
        << "Alone: " << aloneVertexCount << ", With neighbor: " << withNeighborVertexCount;
}

// =============================================================================
// ChunkActivityTracker Neighbor Notification Tests
// =============================================================================

class ChunkActivityTrackerNeighborTest : public ::testing::Test {
  protected:
    ChunkActivityTracker tracker;
};

TEST_F(ChunkActivityTrackerNeighborTest, NotifyBoundaryChangeWakesSleepingChunk) {
    // A sleeping chunk should become BoundaryDirty when notified
    tracker.setState({0, 0, 0}, ChunkState::Sleeping);
    tracker.notifyBoundaryChange({0, 0, 0});

    EXPECT_EQ(tracker.getState({0, 0, 0}), ChunkState::BoundaryDirty);
}

TEST_F(ChunkActivityTrackerNeighborTest, NotifyBoundaryChangeKeepsActiveActive) {
    // An active chunk should become BoundaryDirty when notified
    // (so it can re-mesh its boundary)
    tracker.setState({0, 0, 0}, ChunkState::Active);
    tracker.notifyBoundaryChange({0, 0, 0});

    EXPECT_EQ(tracker.getState({0, 0, 0}), ChunkState::BoundaryDirty);
}

TEST_F(ChunkActivityTrackerNeighborTest, CollectActiveIncludesBoundaryDirty) {
    tracker.setState({0, 0, 0}, ChunkState::Sleeping);
    tracker.setState({1, 0, 0}, ChunkState::Active);
    tracker.notifyBoundaryChange({0, 0, 0}); // Makes it BoundaryDirty

    auto active = tracker.collectActiveChunks();
    EXPECT_EQ(active.size(), 2u); // Both Active and BoundaryDirty
}

// =============================================================================
// Density Blur Cross-Chunk Tests
// Tests for the 5x5x5 blur in VoxelMeshingSystem::meshChunk()
// =============================================================================

class DensityBlurCrossChunkTest : public ::testing::Test {
  protected:
    // Helper to compute what the blur would produce at a given position
    // This mirrors the blur logic in VoxelMeshingSystem::meshChunk()
    float compute5x5x5Blur(const fabric::ChunkedGrid<float>& grid, int wx, int wy, int wz) {
        float sum = 0.0f;
        int count = 0;
        for (int nz = -2; nz <= 2; ++nz) {
            for (int ny = -2; ny <= 2; ++ny) {
                for (int nx = -2; nx <= 2; ++nx) {
                    sum += grid.get(wx + nx, wy + ny, wz + nz);
                    ++count;
                }
            }
        }
        return sum / static_cast<float>(count);
    }
};

TEST_F(DensityBlurCrossChunkTest, BlurAtChunkBoundaryWithMissingNeighbor) {
    // Create a density grid where chunk (0,0,0) is solid (density 1.0)
    // and chunk (1,0,0) is missing (ChunkedGrid returns 0.0 for missing)

    fabric::ChunkedGrid<float> densityGrid;

    // Fill chunk (0,0,0) with density 1.0
    for (int z = 0; z < 32; ++z) {
        for (int y = 0; y < 32; ++y) {
            for (int x = 0; x < 32; ++x) {
                densityGrid.set(x, y, z, 1.0f);
            }
        }
    }

    // At the boundary x=31, the blur kernel samples from x=29..33
    // x=29,30,31 are in chunk (0,0,0) -> density 1.0
    // x=32,33 are in chunk (1,0,0) -> missing, returns 0.0
    // So blur = (3*1.0 + 2*0.0) / 5 = 0.6 per axis
    // For 5x5x5 = 125 samples: 3*5*5=75 solid, 2*5*5=50 air -> 75/125 = 0.6

    float boundaryBlur = compute5x5x5Blur(densityGrid, 31, 16, 16);

    // The blur should be less than 1.0 due to including air samples
    // This documents the behavior that causes chamfering at boundaries
    EXPECT_LT(boundaryBlur, 1.0f) << "Blur at boundary should be < 1.0 when neighbor chunk is missing. "
                                  << "Actual: " << boundaryBlur;

    // Interior voxels should have blur = 1.0 (all neighbors solid)
    float interiorBlur = compute5x5x5Blur(densityGrid, 16, 16, 16);
    EXPECT_NEAR(interiorBlur, 1.0f, 0.001f) << "Interior blur should be 1.0 when fully surrounded by solid. "
                                            << "Actual: " << interiorBlur;
}

TEST_F(DensityBlurCrossChunkTest, BlurWithSolidNeighborHasHigherDensity) {
    // Compare boundary blur with and without solid neighbor

    // Case A: No neighbor (air beyond boundary)
    fabric::ChunkedGrid<float> gridNoNeighbor;
    for (int z = 0; z < 32; ++z) {
        for (int y = 0; y < 32; ++y) {
            for (int x = 0; x < 32; ++x) {
                gridNoNeighbor.set(x, y, z, 1.0f);
            }
        }
    }
    float densityNoNeighbor = compute5x5x5Blur(gridNoNeighbor, 31, 16, 16);

    // Case B: Solid neighbor at +X (chunk 1,0,0)
    fabric::ChunkedGrid<float> gridWithNeighbor;
    for (int z = 0; z < 32; ++z) {
        for (int y = 0; y < 32; ++y) {
            for (int x = 0; x < 64; ++x) { // Both chunks
                gridWithNeighbor.set(x, y, z, 1.0f);
            }
        }
    }
    float densityWithNeighbor = compute5x5x5Blur(gridWithNeighbor, 31, 16, 16);

    // With solid neighbor, density should be higher (closer to 1.0)
    EXPECT_GT(densityWithNeighbor, densityNoNeighbor)
        << "Blur with solid neighbor should be higher than with air. "
        << "No neighbor: " << densityNoNeighbor << ", With neighbor: " << densityWithNeighbor;

    // With solid neighbor at boundary, blur should be exactly 1.0
    EXPECT_NEAR(densityWithNeighbor, 1.0f, 0.001f) << "Blur with solid neighbor should be 1.0. "
                                                   << "Actual: " << densityWithNeighbor;
}

// =============================================================================
// Integration Test: Chunk Loading Sequence
// =============================================================================

class ChunkLoadingSequenceTest : public ::testing::Test {
  protected:
    SimulationGrid simGrid;
    ChunkActivityTracker tracker;
    VoxelMeshingSystem meshingSystem;

    void SetUp() override {
        meshingSystem.setSimulationGrid(&simGrid);
        meshingSystem.setActivityTracker(&tracker);
        tracker.setReferencePoint(0, 0, 0);
        // These tests verify notification behavior when neighbors load;
        // they need to mesh chunks before all neighbors exist.
        meshingSystem.setRequireNeighborsForMeshing(false);
    }

    void fillChunkWithVoxels(int cx, int cy, int cz) {
        int baseX = cx * K_CHUNK_SIZE;
        int baseY = cy * K_CHUNK_SIZE;
        int baseZ = cz * K_CHUNK_SIZE;

        VoxelCell solid{recurse::simulation::material_ids::STONE, 0, 0};
        for (int z = 0; z < K_CHUNK_SIZE; ++z) {
            for (int y = 0; y < K_CHUNK_SIZE; ++y) {
                for (int x = 0; x < K_CHUNK_SIZE; ++x) {
                    simGrid.writeCell(baseX + x, baseY + y, baseZ + z, solid);
                }
            }
        }
        simGrid.advanceEpoch();
    }
};

TEST_F(ChunkLoadingSequenceTest, SequentialLoadingShouldUpdateExistingChunkMeshes) {
    // This test documents the BUG:
    // Load chunk A, mesh it (boundary reads air)
    // Load chunk B adjacent to A
    // EXPECTED: A's mesh updates because B is now solid
    // ACTUAL: A's mesh stays unchanged (chamfered boundary)

    // Phase 1: Load and mesh chunk (0,0,0) alone
    fillChunkWithVoxels(0, 0, 0);
    tracker.setState({0, 0, 0}, ChunkState::Active);
    meshingSystem.processFrame();

    const auto& meshes = meshingSystem.gpuMeshes();
    ASSERT_EQ(meshes.count(ChunkCoord{0, 0, 0}), 1u);
    size_t isolatedVertexCount = meshes.at(ChunkCoord{0, 0, 0}).vertexCount;

    // Phase 2: Load adjacent chunk (1,0,0)
    fillChunkWithVoxels(1, 0, 0);
    tracker.setState({1, 0, 0}, ChunkState::Active);

    // BUG: Without calling notifyBoundaryChange({0,0,0}), chunk (0,0,0) won't remesh
    // This test will FAIL until the fix is applied
    // tracker.notifyBoundaryChange({0, 0, 0});  // Uncomment after fix

    meshingSystem.processFrame();

    // Both chunks should have meshes
    EXPECT_EQ(meshes.count(ChunkCoord{0, 0, 0}), 1u);
    EXPECT_EQ(meshes.count(ChunkCoord{1, 0, 0}), 1u);

    // IMPORTANT: After fix, chunk (0,0,0) should have been remeshed
    // With proper neighbor notification, the vertex count should change
    // because the face at x=32 is now interior (solid-solid) not boundary (solid-air)

    // This assertion documents expected behavior:
    // When chunk (1,0,0) loads, chunk (0,0,0)'s boundary face becomes interior
    // so it should have FEWER vertices (no surface mesh needed at that face)
    //
    // NOTE: This test will FAIL until VoxelSimulationSystem::generateChunk()
    // calls wakeNeighbors() or notifyBoundaryChange()

    // Uncomment this assertion after the fix is applied:
    // EXPECT_LT(meshes.at(ChunkCoord{0, 0, 0}).vertexCount, isolatedVertexCount)
    //     << "Chunk (0,0,0) should have fewer vertices after neighbor loads "
    //     << "(boundary face becomes interior). "
    //     << "Before: " << isolatedVertexCount << ", After: " << meshes.at(ChunkCoord{0, 0, 0}).vertexCount;
}

TEST_F(ChunkLoadingSequenceTest, AllSixNeighborsAffectMesh) {
    // Test that loading any of the 6 face-adjacent neighbors affects the mesh

    fillChunkWithVoxels(0, 0, 0);
    tracker.setState({0, 0, 0}, ChunkState::Active);
    meshingSystem.processFrame();

    size_t isolatedCount = meshingSystem.gpuMeshes().at(ChunkCoord{0, 0, 0}).vertexCount;

    // Define the 6 face-adjacent neighbor offsets
    struct NeighborDef {
        int dx, dy, dz;
    };
    NeighborDef neighbors[] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};

    // Test each neighbor direction
    for (const auto& n : neighbors) {
        // Fresh setup for each test
        SimulationGrid testGrid;
        ChunkActivityTracker testTracker;
        VoxelMeshingSystem testSystem;
        testSystem.setSimulationGrid(&testGrid);
        testSystem.setActivityTracker(&testTracker);
        testSystem.setRequireNeighborsForMeshing(false); // Mesh without full neighbor set
        testTracker.setReferencePoint(0, 0, 0);

        // Fill center chunk
        VoxelCell solid{recurse::simulation::material_ids::STONE, 0, 0};
        for (int z = 0; z < K_CHUNK_SIZE; ++z) {
            for (int y = 0; y < K_CHUNK_SIZE; ++y) {
                for (int x = 0; x < K_CHUNK_SIZE; ++x) {
                    testGrid.writeCell(x, y, z, solid);
                }
            }
        }
        testGrid.advanceEpoch();
        testTracker.setState({0, 0, 0}, ChunkState::Active);
        testSystem.processFrame();

        size_t beforeCount = testSystem.gpuMeshes().at(ChunkCoord{0, 0, 0}).vertexCount;

        // Fill neighbor chunk
        int ncx = n.dx, ncy = n.dy, ncz = n.dz;
        for (int z = 0; z < K_CHUNK_SIZE; ++z) {
            for (int y = 0; y < K_CHUNK_SIZE; ++y) {
                for (int x = 0; x < K_CHUNK_SIZE; ++x) {
                    testGrid.writeCell(ncx * K_CHUNK_SIZE + x, ncy * K_CHUNK_SIZE + y, ncz * K_CHUNK_SIZE + z, solid);
                }
            }
        }
        testGrid.advanceEpoch();
        testTracker.setState({ncx, ncy, ncz}, ChunkState::Active);

        // Notify center chunk (fix required)
        testTracker.notifyBoundaryChange({0, 0, 0});

        testSystem.processFrame();

        size_t afterCount = testSystem.gpuMeshes().at(ChunkCoord{0, 0, 0}).vertexCount;

        // With proper notification, vertex count should decrease
        // (one boundary face becomes interior)
        EXPECT_LT(afterCount, beforeCount)
            << "Loading neighbor (" << n.dx << "," << n.dy << "," << n.dz << ") should reduce center chunk vertices. "
            << "Before: " << beforeCount << ", After: " << afterCount;
    }
}
