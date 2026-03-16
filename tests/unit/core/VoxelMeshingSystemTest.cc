#include "recurse/systems/VoxelMeshingSystem.hh"

#include "fabric/world/ChunkedGrid.hh"
#include "recurse/simulation/ChunkActivityTracker.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelMaterial.hh"

#include <gtest/gtest.h>

using fabric::ChunkCoord;
using recurse::simulation::ChunkActivityTracker;
using recurse::simulation::ChunkState;
using recurse::simulation::K_CHUNK_SIZE;
using recurse::simulation::SimulationGrid;
using recurse::simulation::VoxelCell;
namespace MaterialIds = recurse::simulation::material_ids;
using recurse::systems::VoxelMeshingSystem;

class VoxelMeshingSystemTest : public ::testing::Test {
  protected:
    SimulationGrid simGrid;
    ChunkActivityTracker tracker;
    VoxelMeshingSystem system;

    void SetUp() override {
        system.setSimulationGrid(&simGrid);
        system.setActivityTracker(&tracker);
        tracker.setReferencePoint(0, 0, 0);
        // Unit tests don't set up neighbors; bypass the neighbor check.
        system.setRequireNeighborsForMeshing(false);
    }

    void fillChunkSolid(const ChunkCoord& coord) {
        VoxelCell solid;
        solid.materialId = MaterialIds::STONE;
        simGrid.fillChunk(coord.x, coord.y, coord.z, solid);
    }
};

TEST_F(VoxelMeshingSystemTest, SystemLifecycle_InitRenderShutdown) {
    system.processFrame();
    system.shutdown();
}

TEST_F(VoxelMeshingSystemTest, DirtyChunkConsumed) {
    ChunkCoord coord{0, 0, 0};
    fillChunkSolid(coord);
    // BoundaryDirty: meshing should process and then sleep the chunk.
    // Active chunks are NOT slept by meshing (FallingSandSystem owns that).
    tracker.setState({coord.x, coord.y, coord.z}, ChunkState::BoundaryDirty);

    system.processFrame();

    EXPECT_EQ(tracker.getState({coord.x, coord.y, coord.z}), ChunkState::Sleeping);
    const auto& meshes = system.gpuMeshes();
    ASSERT_EQ(meshes.count(coord), 1u);
    EXPECT_TRUE(meshes.at(coord).valid);
    EXPECT_GT(meshes.at(coord).vertexCount, 0u);
}

TEST_F(VoxelMeshingSystemTest, BudgetLimitsChunksPerFrame) {
    system.setMeshBudget(3);

    for (int i = 0; i < 10; ++i) {
        ChunkCoord coord{i, 0, 0};
        fillChunkSolid(coord);
        tracker.setState({coord.x, coord.y, coord.z}, ChunkState::BoundaryDirty);
    }

    system.processFrame();

    EXPECT_EQ(system.gpuMeshes().size(), 3u);
    // 3 meshed chunks slept (BoundaryDirty -> Sleeping), 7 remain
    EXPECT_EQ(tracker.collectActiveChunks().size(), 7u);
}

TEST_F(VoxelMeshingSystemTest, PriorityOrdering) {
    ChunkCoord near{1, 0, 0};
    ChunkCoord mid{5, 0, 0};
    ChunkCoord far{10, 0, 0};

    fillChunkSolid(near);
    fillChunkSolid(mid);
    fillChunkSolid(far);

    tracker.setState({near.x, near.y, near.z}, ChunkState::Active);
    tracker.setState({mid.x, mid.y, mid.z}, ChunkState::Active);
    tracker.setState({far.x, far.y, far.z}, ChunkState::Active);

    system.setMeshBudget(2);
    system.processFrame();

    const auto& meshes = system.gpuMeshes();
    EXPECT_EQ(meshes.count(near), 1u);
    EXPECT_EQ(meshes.count(mid), 1u);
    EXPECT_EQ(meshes.count(far), 0u);
    EXPECT_NE(tracker.getState({far.x, far.y, far.z}), ChunkState::Sleeping);
}

TEST_F(VoxelMeshingSystemTest, EmptyChunkNoMesh) {
    ChunkCoord coord{0, 0, 0};
    tracker.setState({coord.x, coord.y, coord.z}, ChunkState::BoundaryDirty);

    system.processFrame();

    EXPECT_EQ(system.gpuMeshes().count(coord), 0u);
    EXPECT_EQ(tracker.getState({coord.x, coord.y, coord.z}), ChunkState::Sleeping);
}

TEST_F(VoxelMeshingSystemTest, MeshUpdateReplacesOld) {
    ChunkCoord coord{0, 0, 0};
    fillChunkSolid(coord);
    tracker.setState({coord.x, coord.y, coord.z}, ChunkState::Active);
    system.processFrame();

    const auto& meshes = system.gpuMeshes();
    ASSERT_EQ(meshes.count(coord), 1u);
    EXPECT_TRUE(meshes.at(coord).valid);

    // Re-mark dirty and remesh -- entry should be replaced, not duplicated
    tracker.setState({coord.x, coord.y, coord.z}, ChunkState::Active);
    system.processFrame();

    EXPECT_EQ(meshes.count(coord), 1u);
    EXPECT_TRUE(meshes.at(coord).valid);
}

TEST_F(VoxelMeshingSystemTest, KnownVoxelFieldMesh) {
    ChunkCoord coord{0, 0, 0};
    VoxelCell solid;
    solid.materialId = MaterialIds::STONE;

    // Fill bottom half (y < 16) with stone, top half remains air
    for (int z = 0; z < K_CHUNK_SIZE; ++z)
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < K_CHUNK_SIZE; ++x)
                simGrid.writeCell(x, y, z, solid);
    simGrid.advanceEpoch();

    tracker.setState({coord.x, coord.y, coord.z}, ChunkState::Active);
    system.processFrame();

    const auto& meshes = system.gpuMeshes();
    ASSERT_EQ(meshes.count(coord), 1u);
    const auto& mesh = meshes.at(coord);
    EXPECT_TRUE(mesh.valid);
    EXPECT_GT(mesh.vertexCount, 0u);
    EXPECT_GT(mesh.indexCount, 0u);
}
