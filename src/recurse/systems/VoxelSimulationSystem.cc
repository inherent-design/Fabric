#include "recurse/systems/VoxelSimulationSystem.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/Event.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/simulation/VoxelSimulationSystem.hh"
#include "fabric/utils/Profiler.hh"
#include "recurse/gameplay/VoxelInteraction.hh"
#include "recurse/systems/TerrainSystem.hh"
#include "recurse/world/WorldGenerator.hh"

namespace recurse::systems {

VoxelSimulationSystem::VoxelSimulationSystem() = default;
VoxelSimulationSystem::~VoxelSimulationSystem() = default;

void VoxelSimulationSystem::init(fabric::AppContext& ctx) {
    terrain_ = ctx.systemRegistry.get<TerrainSystem>();
    dispatcher_ = &ctx.dispatcher;
    fabSim_ = std::make_unique<fabric::simulation::VoxelSimulationSystem>();

    // Defer terrain generation until TerrainSystem has generated the world
    // This allows MainMenuSystem to select world type first
    FABRIC_LOG_INFO("VoxelSimulationSystem initialized (awaiting world generation)");
}

void VoxelSimulationSystem::shutdown() {
    fabSim_.reset();
    terrain_ = nullptr;
    dispatcher_ = nullptr;
}

void VoxelSimulationSystem::fixedUpdate(fabric::AppContext& /*ctx*/, float /*fixedDt*/) {
    FABRIC_ZONE_SCOPED_N("recurse_voxel_sim");
    if (!fabSim_)
        return;

    lastActiveCount_ = fabSim_->activityTracker().collectActiveChunks().size();
    FABRIC_ZONE_VALUE(static_cast<int64_t>(lastActiveCount_));
    fabSim_->tick();
}

void VoxelSimulationSystem::configureDependencies() {
    after<TerrainSystem>();
}

void VoxelSimulationSystem::generateInitialWorld() {
    if (!terrain_ || !fabSim_ || !dispatcher_) {
        FABRIC_LOG_ERROR("VoxelSimulationSystem: Cannot generate world - not initialized");
        return;
    }

    // Generate initial terrain into the simulation grid.
    // Use 5x3x5 region so inner 3x3x1 chunks at y=0 have all 6 neighbors for meshing.
    auto& gen = terrain_->worldGenerator();
    auto& grid = fabSim_->grid();
    auto& tracker = fabSim_->activityTracker();

    // Track which chunks we generate for collision initialization
    std::vector<std::tuple<int, int, int>> generatedChunks;

    for (int cz = -2; cz <= 2; ++cz) {
        for (int cy = -1; cy <= 1; ++cy) {
            for (int cx = -2; cx <= 2; ++cx) {
                gen.generate(grid, cx, cy, cz);
                // Mark chunk Active so VoxelMeshingSystem will mesh it
                tracker.setState(fabric::simulation::ChunkPos{cx, cy, cz}, fabric::simulation::ChunkState::Active);
                generatedChunks.emplace_back(cx, cy, cz);
            }
        }
    }
    grid.advanceEpoch();

    // Sync density field from simulation grid for collision detection
    terrain_->syncDensityFromGrid(grid);

    // Initialize Jolt physics collision for each chunk
    for (const auto& [cx, cy, cz] : generatedChunks) {
        fabric::Event e(kVoxelChangedEvent, "VoxelSimulationSystem");
        e.setData("cx", cx);
        e.setData("cy", cy);
        e.setData("cz", cz);
        dispatcher_->dispatchEvent(e);
    }

    FABRIC_LOG_INFO("VoxelSimulationSystem: World generated ({} chunks in grid, collision synced)",
                    grid.allChunks().size());
}

fabric::simulation::SimulationGrid& VoxelSimulationSystem::simulationGrid() {
    return fabSim_->grid();
}

const fabric::simulation::SimulationGrid& VoxelSimulationSystem::simulationGrid() const {
    return fabSim_->grid();
}

fabric::simulation::ChunkActivityTracker& VoxelSimulationSystem::activityTracker() {
    return fabSim_->activityTracker();
}

const fabric::simulation::ChunkActivityTracker& VoxelSimulationSystem::activityTracker() const {
    return fabSim_->activityTracker();
}

void VoxelSimulationSystem::generateChunk(int cx, int cy, int cz) {
    if (!terrain_ || !fabSim_)
        return;
    auto& gen = terrain_->worldGenerator();
    gen.generate(fabSim_->grid(), cx, cy, cz);
    fabSim_->grid().advanceEpoch();
    fabSim_->activityTracker().setState(fabric::simulation::ChunkPos{cx, cy, cz},
                                        fabric::simulation::ChunkState::Active);

    // Sync density for this chunk to TerrainSystem's density field
    // (sync just this chunk, not all chunks)
    constexpr int K_CHUNK_SIZE = 32;
    int baseX = cx * K_CHUNK_SIZE;
    int baseY = cy * K_CHUNK_SIZE;
    int baseZ = cz * K_CHUNK_SIZE;

    // Sync density - handle both materialized chunks and sentinels (non-materialized)
    using namespace fabric::simulation;
    const auto* readBuf = fabSim_->grid().readBuffer(cx, cy, cz);
    if (readBuf) {
        // Materialized chunk: read from buffer
        for (int lz = 0; lz < K_CHUNK_SIZE; ++lz) {
            for (int ly = 0; ly < K_CHUNK_SIZE; ++ly) {
                for (int lx = 0; lx < K_CHUNK_SIZE; ++lx) {
                    size_t idx = static_cast<size_t>(lx + ly * K_CHUNK_SIZE + lz * K_CHUNK_SIZE * K_CHUNK_SIZE);
                    const VoxelCell& cell = (*readBuf)[idx];
                    float density = (cell.materialId == material_ids::AIR) ? 0.0f : 1.0f;
                    terrain_->densityGrid().set(baseX + lx, baseY + ly, baseZ + lz, density);
                }
            }
        }
    } else if (fabSim_->grid().hasChunk(cx, cy, cz)) {
        // Sentinel chunk (not materialized): use fill value for uniform density
        VoxelCell fillValue = fabSim_->grid().getChunkFillValue(cx, cy, cz);
        float density = (fillValue.materialId == material_ids::AIR) ? 0.0f : 1.0f;
        for (int lz = 0; lz < K_CHUNK_SIZE; ++lz) {
            for (int ly = 0; ly < K_CHUNK_SIZE; ++ly) {
                for (int lx = 0; lx < K_CHUNK_SIZE; ++lx) {
                    terrain_->densityGrid().set(baseX + lx, baseY + ly, baseZ + lz, density);
                }
            }
        }
    }

    // Initialize Jolt physics collision for this chunk (always dispatch, even for sentinels)
    if (dispatcher_) {
        fabric::Event e(kVoxelChangedEvent, "VoxelSimulationSystem");
        e.setData("cx", cx);
        e.setData("cy", cy);
        e.setData("cz", cz);
        dispatcher_->dispatchEvent(e);
    }

    // Notify all face-adjacent AND diagonal (in X/Z plane) existing chunks so they re-mesh boundaries.
    // Corner vertices sample across diagonal chunks, so diagonal neighbors must also remesh.
    // 6 face-adjacent: ±X, ±Y, ±Z
    // 4 X/Z diagonal: (±X, ±Z) combinations (Y is same since flat terrain)
    constexpr int K_NEIGHBOR_OFFSETS[10][3] = {// Face-adjacent (6)
                                               {1, 0, 0},
                                               {-1, 0, 0},
                                               {0, 1, 0},
                                               {0, -1, 0},
                                               {0, 0, 1},
                                               {0, 0, -1},
                                               // X/Z diagonal (4) - corner vertices sample across these
                                               {1, 0, 1},
                                               {1, 0, -1},
                                               {-1, 0, 1},
                                               {-1, 0, -1}};
    int notifiedCount = 0;
    for (int d = 0; d < 10; ++d) {
        fabric::simulation::ChunkPos neighbor{cx + K_NEIGHBOR_OFFSETS[d][0], cy + K_NEIGHBOR_OFFSETS[d][1],
                                              cz + K_NEIGHBOR_OFFSETS[d][2]};
        if (fabSim_->grid().hasChunk(neighbor.x, neighbor.y, neighbor.z)) {
            fabSim_->activityTracker().notifyBoundaryChange(neighbor);
            ++notifiedCount;
        }
    }
    if (notifiedCount > 0) {
        FABRIC_LOG_DEBUG("Notified {} existing neighbors (face+diagonal) after loading chunk ({},{},{})", notifiedCount,
                         cx, cy, cz);
    }
}

void VoxelSimulationSystem::removeChunk(int cx, int cy, int cz) {
    if (!fabSim_)
        return;
    fabSim_->grid().removeChunk(cx, cy, cz);
    fabSim_->activityTracker().remove(fabric::simulation::ChunkPos{cx, cy, cz});
}

void VoxelSimulationSystem::resetWorld() {
    if (!fabSim_)
        return;

    // Clear all simulation state
    fabSim_->grid().clear();
    fabSim_->activityTracker().clear();
    lastActiveCount_ = 0;

    FABRIC_LOG_INFO("VoxelSimulationSystem: World reset (grid and tracker cleared)");
}

} // namespace recurse::systems
