#define private public
#include "recurse/systems/VoxelMeshingSystem.hh"
#include "recurse/simulation/CellAccessors.hh"
#undef private

#include "fabric/world/ChunkedGrid.hh"
#include "recurse/simulation/ChunkActivityTracker.hh"
#include "recurse/simulation/EssenceColor.hh"
#include "recurse/simulation/MaterialRegistry.hh"
#include "recurse/simulation/SimulationGrid.hh"
#include "recurse/simulation/VoxelMaterial.hh"

#include <cmath>
#include <gtest/gtest.h>

using fabric::ChunkCoord;
using recurse::simulation::ChunkActivityTracker;
using recurse::simulation::ChunkState;
using recurse::simulation::K_CHUNK_SIZE;
using recurse::simulation::SimulationGrid;
using recurse::simulation::VoxelCell;
namespace MaterialIds = recurse::simulation::material_ids;
using recurse::simulation::cellForMaterial;
using recurse::simulation::cellMaterialId;
using recurse::systems::VoxelMeshingSystem;

class VoxelMeshingSystemTest : public ::testing::Test {
  protected:
    SimulationGrid simGrid;
    ChunkActivityTracker tracker;
    VoxelMeshingSystem system;
    recurse::simulation::MaterialRegistry materials;

    void SetUp() override {
        system.setSimulationGrid(&simGrid);
        system.setActivityTracker(&tracker);
        system.materials_ = &materials;
        tracker.setReferencePoint(0, 0, 0);
        // Unit tests don't set up neighbors; bypass the neighbor check.
        system.setRequireNeighborsForMeshing(false);
    }

    void fillChunkSolid(const ChunkCoord& coord) {
        VoxelCell solid = cellForMaterial(MaterialIds::STONE);
        simGrid.fillChunk(coord.x, coord.y, coord.z, solid);
    }
};

TEST_F(VoxelMeshingSystemTest, SystemLifecycle_InitRenderShutdown) {
    system.processFrame();
    system.shutdown();
}

TEST_F(VoxelMeshingSystemTest, NearChunkMesherDefaultsToGreedy) {
    EXPECT_EQ(system.nearChunkMesher(), VoxelMeshingSystem::NearChunkMesher::Greedy);
}

TEST_F(VoxelMeshingSystemTest, GreedyDefaultDoesNotPreinitializeSnapMCFallback) {
    EXPECT_EQ(system.nearChunkMesher(), VoxelMeshingSystem::NearChunkMesher::Greedy);
    EXPECT_EQ(system.snapMcMesher_, nullptr);
}

TEST_F(VoxelMeshingSystemTest, SelectingSnapMCLazilyInstantiatesExperimentalFallback) {
    EXPECT_EQ(system.snapMcMesher_, nullptr);

    system.setNearChunkMesher(VoxelMeshingSystem::NearChunkMesher::SnapMC);

    EXPECT_EQ(system.nearChunkMesher(), VoxelMeshingSystem::NearChunkMesher::SnapMC);
    EXPECT_NE(system.snapMcMesher_, nullptr);
}

TEST_F(VoxelMeshingSystemTest, GreedySelectionLeavesAirChunkEmpty) {
    ChunkCoord coord{0, 0, 0};
    system.setNearChunkMesher(VoxelMeshingSystem::NearChunkMesher::Greedy);
    simGrid.fillChunk(coord.x, coord.y, coord.z, VoxelCell{});

    const auto result = system.generateMeshCPU(coord);

    EXPECT_FALSE(result.valid);
    EXPECT_TRUE(result.vertices.empty());
    EXPECT_TRUE(result.indices.empty());
}

TEST_F(VoxelMeshingSystemTest, GreedySelectionMergesSolidChunkIntoSixQuads) {
    ChunkCoord coord{0, 0, 0};
    fillChunkSolid(coord);
    system.setNearChunkMesher(VoxelMeshingSystem::NearChunkMesher::Greedy);

    const auto result = system.generateMeshCPU(coord);

    ASSERT_TRUE(result.valid);
    EXPECT_EQ(result.vertices.size(), 24u);
    EXPECT_EQ(result.voxelVertices.size(), 24u);
    EXPECT_EQ(result.indices.size(), 36u);
    ASSERT_EQ(result.palette.size(), 1u);
    EXPECT_EQ(result.meshFormat, recurse::ChunkMesh::VertexFormat::Voxel);
}

TEST_F(VoxelMeshingSystemTest, GreedySelectionSuppressesCrossChunkBoundaryFaces) {
    ChunkCoord coord{0, 0, 0};
    fillChunkSolid(coord);
    fillChunkSolid({1, 0, 0});
    system.setNearChunkMesher(VoxelMeshingSystem::NearChunkMesher::Greedy);

    const auto result = system.generateMeshCPU(coord);

    ASSERT_TRUE(result.valid);
    EXPECT_EQ(result.vertices.size(), 20u);
    EXPECT_EQ(result.indices.size(), 30u);
}

TEST_F(VoxelMeshingSystemTest, GreedySelectionLeavesSingleExposedFaceAsOneQuad) {
    ChunkCoord coord{0, 0, 0};
    fillChunkSolid(coord);
    fillChunkSolid({1, 0, 0});
    fillChunkSolid({-1, 0, 0});
    fillChunkSolid({0, 1, 0});
    fillChunkSolid({0, -1, 0});
    fillChunkSolid({0, 0, 1});
    system.setNearChunkMesher(VoxelMeshingSystem::NearChunkMesher::Greedy);

    const auto result = system.generateMeshCPU(coord);

    ASSERT_TRUE(result.valid);
    ASSERT_EQ(result.vertices.size(), 4u);
    EXPECT_EQ(result.indices.size(), 6u);
    for (const auto& vertex : result.vertices) {
        EXPECT_FLOAT_EQ(vertex.nx, 0.0f);
        EXPECT_FLOAT_EQ(vertex.ny, 0.0f);
        EXPECT_FLOAT_EQ(vertex.nz, -1.0f);
    }
}

TEST_F(VoxelMeshingSystemTest, GreedySelectionPreservesPaletteContractAndShaderAOPacking) {
    ChunkCoord coord{0, 0, 0};
    VoxelCell sand = cellForMaterial(MaterialIds::SAND);
    simGrid.fillChunk(coord.x, coord.y, coord.z, sand);

    auto* palette = simGrid.chunkPalette(coord.x, coord.y, coord.z);
    ASSERT_NE(palette, nullptr);
    palette->addEntryRaw({0.0f, 0.0f, 1.0f, 0.0f});

    system.setNearChunkMesher(VoxelMeshingSystem::NearChunkMesher::Greedy);

    const auto result = system.generateMeshCPU(coord);

    ASSERT_TRUE(result.valid);
    ASSERT_EQ(result.palette.size(), 1u);

    const auto expected = materials.terrainAppearanceColor(MaterialIds::SAND);
    EXPECT_FLOAT_EQ(result.palette[0][0], expected[0]);
    EXPECT_FLOAT_EQ(result.palette[0][1], expected[1]);
    EXPECT_FLOAT_EQ(result.palette[0][2], expected[2]);
    EXPECT_FLOAT_EQ(result.palette[0][3], expected[3]);

    for (const auto& vertex : result.vertices) {
        EXPECT_EQ(vertex.getMaterialId(), 0u);
        EXPECT_EQ(vertex.getAO(), recurse::SmoothVoxelVertex::K_SHADER_DEFAULT_AO);
    }

    for (const auto& vertex : result.voxelVertices) {
        EXPECT_EQ(vertex.paletteIndex(), 0u);
        EXPECT_EQ(vertex.aoLevel(), 3u);
    }
}

TEST_F(VoxelMeshingSystemTest, GreedyDefaultStillMeshesThroughProcessFrame) {
    ChunkCoord coord{0, 0, 0};
    fillChunkSolid(coord);

    tracker.setState({coord.x, coord.y, coord.z}, ChunkState::BoundaryDirty);
    system.processFrame();

    const auto& greedyMesh = system.gpuMeshes().at(coord);
    EXPECT_EQ(system.nearChunkMesher(), VoxelMeshingSystem::NearChunkMesher::Greedy);
    EXPECT_TRUE(greedyMesh.valid);
    EXPECT_EQ(greedyMesh.vertexCount, 24u);
    EXPECT_EQ(greedyMesh.indexCount, 36u);
    EXPECT_EQ(greedyMesh.mesh.vertexFormat, recurse::ChunkMesh::VertexFormat::Voxel);
    EXPECT_EQ(greedyMesh.mesh.vertexStrideBytes, sizeof(recurse::VoxelVertex));
}

TEST_F(VoxelMeshingSystemTest, SnapMCSelectionRemainsAvailableForRollback) {
    ChunkCoord coord{0, 0, 0};
    fillChunkSolid(coord);
    system.setNearChunkMesher(VoxelMeshingSystem::NearChunkMesher::SnapMC);

    tracker.setState({coord.x, coord.y, coord.z}, ChunkState::BoundaryDirty);
    system.processFrame();

    const auto& snapMcMesh = system.gpuMeshes().at(coord);
    EXPECT_EQ(system.nearChunkMesher(), VoxelMeshingSystem::NearChunkMesher::SnapMC);
    EXPECT_TRUE(snapMcMesh.valid);
    EXPECT_GT(snapMcMesh.vertexCount, 0u);
    EXPECT_GT(snapMcMesh.indexCount, 0u);
    EXPECT_EQ(snapMcMesh.mesh.vertexFormat, recurse::ChunkMesh::VertexFormat::Smooth);
    EXPECT_EQ(snapMcMesh.mesh.vertexStrideBytes, sizeof(recurse::SmoothVoxelVertex));
}

TEST_F(VoxelMeshingSystemTest, GreedyDefaultUsesNoMoreGeometryThanSnapMCRollbackForSolidChunk) {
    ChunkCoord coord{0, 0, 0};
    fillChunkSolid(coord);

    const auto greedy = system.generateMeshCPU(coord);
    ASSERT_TRUE(greedy.valid);

    system.setNearChunkMesher(VoxelMeshingSystem::NearChunkMesher::SnapMC);
    const auto snapMc = system.generateMeshCPU(coord);

    ASSERT_TRUE(snapMc.valid);
    RecordProperty("greedy_vertices", static_cast<int>(greedy.vertices.size()));
    RecordProperty("greedy_indices", static_cast<int>(greedy.indices.size()));
    RecordProperty("snapmc_vertices", static_cast<int>(snapMc.vertices.size()));
    RecordProperty("snapmc_indices", static_cast<int>(snapMc.indices.size()));
    EXPECT_LE(greedy.vertices.size(), snapMc.vertices.size());
    EXPECT_LE(greedy.indices.size(), snapMc.indices.size());
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
    VoxelCell solid = cellForMaterial(MaterialIds::STONE);

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

TEST_F(VoxelMeshingSystemTest, DebugCountersTrackMeshLifecycleWithoutRescans) {
    ChunkCoord coord{0, 0, 0};
    fillChunkSolid(coord);
    tracker.setState({coord.x, coord.y, coord.z}, ChunkState::BoundaryDirty);

    system.processFrame();

    const auto& mesh = system.gpuMeshes().at(coord);
    EXPECT_EQ(system.pendingMeshCount(), 0u);
    EXPECT_EQ(system.vertexBufferSize(), static_cast<size_t>(mesh.vertexCount));
    EXPECT_EQ(system.indexBufferSize(), static_cast<size_t>(mesh.indexCount));
    EXPECT_EQ(system.vertexBufferBytes(), static_cast<size_t>(mesh.vertexCount) * sizeof(recurse::VoxelVertex));
    EXPECT_EQ(system.indexBufferBytes(), static_cast<size_t>(mesh.indexCount) * sizeof(uint32_t));

    system.removeChunkMesh(coord);
    EXPECT_EQ(system.vertexBufferSize(), 0u);
    EXPECT_EQ(system.indexBufferSize(), 0u);
    EXPECT_EQ(system.vertexBufferBytes(), 0u);
    EXPECT_EQ(system.indexBufferBytes(), 0u);
}

TEST_F(VoxelMeshingSystemTest, FullResPaletteUsesMaterialAppearanceContractNotChunkEssence) {
    ChunkCoord coord{0, 0, 0};
    VoxelCell sand = cellForMaterial(MaterialIds::SAND);
    simGrid.fillChunk(coord.x, coord.y, coord.z, sand);

    auto* palette = simGrid.chunkPalette(coord.x, coord.y, coord.z);
    ASSERT_NE(palette, nullptr);
    palette->addEntryRaw({0.0f, 0.0f, 1.0f, 0.0f});

    tracker.setState({coord.x, coord.y, coord.z}, ChunkState::BoundaryDirty);
    system.processFrame();

    const auto& gpuMesh = system.gpuMeshes().at(coord);
    ASSERT_TRUE(gpuMesh.valid);
    ASSERT_EQ(gpuMesh.mesh.palette.size(), 1u);

    const auto expected = materials.terrainAppearanceColor(MaterialIds::SAND);
    const fabric::Vector4<float, fabric::Space::World> forcedEssence(0.0f, 0.0f, 1.0f, 0.0f);
    const auto essenceColor = recurse::simulation::essenceToColor(forcedEssence);

    EXPECT_FLOAT_EQ(gpuMesh.mesh.palette[0][0], expected[0]);
    EXPECT_FLOAT_EQ(gpuMesh.mesh.palette[0][1], expected[1]);
    EXPECT_FLOAT_EQ(gpuMesh.mesh.palette[0][2], expected[2]);
    EXPECT_FLOAT_EQ(gpuMesh.mesh.palette[0][3], expected[3]);
    EXPECT_GT(std::fabs(gpuMesh.mesh.palette[0][0] - essenceColor[0]), 0.2f);
    EXPECT_GT(std::fabs(gpuMesh.mesh.palette[0][2] - essenceColor[2]), 0.25f);
}
