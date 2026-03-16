#include "recurse/systems/VoxelSimulationSystem.hh"
#include "fabric/world/ChunkCoordUtils.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/Event.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/core/WorldLifecycle.hh"
#include "fabric/log/Log.hh"
#include "fabric/platform/JobScheduler.hh"
#include "fabric/utils/Profiler.hh"
#include "recurse/character/VoxelInteraction.hh"
#include "recurse/simulation/ChunkRegistry.hh"
#include "recurse/simulation/ChunkState.hh"
#include "recurse/simulation/EssenceAssigner.hh"
#include "recurse/simulation/VoxelMaterial.hh"
#include "recurse/simulation/VoxelSimulationSystem.hh"
#include "recurse/systems/ChunkPipelineSystem.hh"
#include "recurse/systems/TerrainSystem.hh"
#include "recurse/world/WorldGenerator.hh"

namespace recurse::systems {

using fabric::K_FACE_DIAGONAL_NEIGHBORS;

VoxelSimulationSystem::VoxelSimulationSystem() = default;
VoxelSimulationSystem::~VoxelSimulationSystem() = default;

void VoxelSimulationSystem::doInit(fabric::AppContext& ctx) {
    if (auto* wl = ctx.worldLifecycle) {
        wl->registerParticipant([this]() { onWorldBegin(); }, [this]() { onWorldEnd(); });
    }
    terrain_ = ctx.systemRegistry.get<TerrainSystem>();
    dispatcher_ = &ctx.dispatcher;
    fabSim_ = std::make_unique<recurse::simulation::VoxelSimulationSystem>();

    // Defer terrain generation until TerrainSystem has generated the world
    // This allows MainMenuSystem to select world type first
    FABRIC_LOG_INFO("VoxelSimulationSystem initialized (awaiting world generation)");
}

void VoxelSimulationSystem::doShutdown() {
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

    // Dispatch collision rebuild for chunks that just settled (simulation
    // moved cells but the event was not emitted during parallel dispatch).
    if (dispatcher_) {
        for (const auto& pos : fabSim_->settledChunks()) {
            fabric::Event e(K_VOXEL_CHANGED_EVENT, "VoxelSimulationSystem");
            e.setData("cx", pos.x);
            e.setData("cy", pos.y);
            e.setData("cz", pos.z);
            e.setData("source", static_cast<int>(ChangeSource::Physics));
            dispatcher_->dispatchEvent(e);
        }

        // Emit per-voxel physics change events for rollback completeness.
        // Settled events (above) remain chunk-level for collision rebuild.
        // Physics detail events carry VoxelChangeDetail for change logging.
        for (const auto& [chunkPos, swaps] : fabSim_->physicsChanges()) {
            if (swaps.empty())
                continue;
            std::vector<VoxelChangeDetail> details;
            details.reserve(swaps.size());
            for (const auto& swap : swaps) {
                VoxelChangeDetail d;
                d.vx = swap.lx;
                d.vy = swap.ly;
                d.vz = swap.lz;
                d.oldCell = swap.oldCell;
                d.newCell = swap.newCell;
                d.playerId = 0;
                d.source = ChangeSource::Physics;
                details.push_back(d);
            }
            emitVoxelChanged(*dispatcher_, chunkPos.x, chunkPos.y, chunkPos.z, std::move(details));
        }
    }
}

void VoxelSimulationSystem::onWorldBegin() {
    // World generation is triggered explicitly by MainMenuSystem.
}

void VoxelSimulationSystem::onWorldEnd() {
    resetWorld();
}

void VoxelSimulationSystem::configureDependencies() {
    after<TerrainSystem>();
    after<ChunkPipelineSystem>();
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
                auto& registry = grid.registry();
                auto absent = recurse::simulation::addChunkRef(registry, cx, cy, cz);
                auto generating =
                    recurse::simulation::transition<recurse::simulation::Absent, recurse::simulation::Generating>(
                        absent, registry);
                gen.generate(grid, cx, cy, cz);
                if (grid.isChunkMaterialized(cx, cy, cz)) {
                    auto* buf = grid.writeBuffer(cx, cy, cz);
                    auto* pal = grid.chunkPalette(cx, cy, cz);
                    if (buf && pal)
                        recurse::simulation::assignEssence(buf->data(), cx, cy, cz, fabSim_->materials(), *pal, 42);
                }
                grid.syncChunkBuffers(cx, cy, cz);
                recurse::simulation::transition<recurse::simulation::Generating, recurse::simulation::Active>(
                    generating, registry);
                tracker.setState(recurse::simulation::ChunkCoord{cx, cy, cz}, recurse::simulation::ChunkState::Active);
                generatedChunks.emplace_back(cx, cy, cz);
            }
        }
    }

    // Initialize Jolt physics collision for each chunk
    for (const auto& [cx, cy, cz] : generatedChunks) {
        fabric::Event e(K_VOXEL_CHANGED_EVENT, "VoxelSimulationSystem");
        e.setData("cx", cx);
        e.setData("cy", cy);
        e.setData("cz", cz);
        e.setData("source", static_cast<int>(ChangeSource::Generation));
        dispatcher_->dispatchEvent(e);
    }

    FABRIC_LOG_INFO("VoxelSimulationSystem: World generated ({} chunks in grid, collision synced)",
                    grid.allChunks().size());
}

recurse::simulation::SimulationGrid& VoxelSimulationSystem::simulationGrid() {
    return fabSim_->grid();
}

const recurse::simulation::SimulationGrid& VoxelSimulationSystem::simulationGrid() const {
    return fabSim_->grid();
}

recurse::simulation::ChunkActivityTracker& VoxelSimulationSystem::activityTracker() {
    return fabSim_->activityTracker();
}

const recurse::simulation::ChunkActivityTracker& VoxelSimulationSystem::activityTracker() const {
    return fabSim_->activityTracker();
}

const recurse::simulation::MaterialRegistry& VoxelSimulationSystem::materials() const {
    return fabSim_->materials();
}

fabric::JobScheduler& VoxelSimulationSystem::scheduler() {
    return fabSim_->scheduler();
}

void VoxelSimulationSystem::generateChunk(int cx, int cy, int cz) {
    if (!terrain_ || !fabSim_)
        return;
    auto& gen = terrain_->worldGenerator();
    auto& grid = fabSim_->grid();
    auto& registry = grid.registry();
    auto absent = recurse::simulation::addChunkRef(registry, cx, cy, cz);
    auto generating =
        recurse::simulation::transition<recurse::simulation::Absent, recurse::simulation::Generating>(absent, registry);
    gen.generate(grid, cx, cy, cz);
    if (grid.isChunkMaterialized(cx, cy, cz)) {
        auto* buf = grid.writeBuffer(cx, cy, cz);
        auto* pal = grid.chunkPalette(cx, cy, cz);
        if (buf && pal)
            recurse::simulation::assignEssence(buf->data(), cx, cy, cz, fabSim_->materials(), *pal, 42);
    }
    grid.syncChunkBuffers(cx, cy, cz);
    recurse::simulation::transition<recurse::simulation::Generating, recurse::simulation::Active>(generating, registry);
    fabSim_->activityTracker().setState(recurse::simulation::ChunkCoord{cx, cy, cz},
                                        recurse::simulation::ChunkState::Active);

    // Initialize Jolt physics collision for this chunk (always dispatch, even for sentinels)
    if (dispatcher_) {
        fabric::Event e(K_VOXEL_CHANGED_EVENT, "VoxelSimulationSystem");
        e.setData("cx", cx);
        e.setData("cy", cy);
        e.setData("cz", cz);
        e.setData("source", static_cast<int>(ChangeSource::Generation));
        dispatcher_->dispatchEvent(e);
    }

    // Notify all face-adjacent AND diagonal (in X/Z plane) existing chunks so they re-mesh boundaries.
    // Corner vertices sample across diagonal chunks, so diagonal neighbors must also remesh.
    int notifiedCount = 0;
    for (int d = 0; d < 10; ++d) {
        recurse::simulation::ChunkCoord neighbor{cx + K_FACE_DIAGONAL_NEIGHBORS[d][0],
                                                 cy + K_FACE_DIAGONAL_NEIGHBORS[d][1],
                                                 cz + K_FACE_DIAGONAL_NEIGHBORS[d][2]};
        if (fabSim_->grid().hasChunk(neighbor.x, neighbor.y, neighbor.z)) {
            fabSim_->activityTracker().notifyBoundaryChange(neighbor);
            ++notifiedCount;
        }
    }
    if (notifiedCount > 0) {
        FABRIC_LOG_TRACE("Notified {} existing neighbors (face+diagonal) after loading chunk ({},{},{})", notifiedCount,
                         cx, cy, cz);
    }
}

void VoxelSimulationSystem::generateChunksBatch(const std::vector<std::tuple<int, int, int>>& chunks) {
    if (!terrain_ || !fabSim_ || chunks.empty())
        return;

    FABRIC_ZONE_SCOPED_N("gen_batch");

    auto& gen = terrain_->worldGenerator();
    auto& grid = fabSim_->grid();
    auto& tracker = fabSim_->activityTracker();
    auto& sched = fabSim_->scheduler();

    // Phase 0: Pre-materialize all chunks on calling thread (structural modification).
    // Resolves registry slots and allocates buffers before parallel dispatch.
    struct GenTask {
        recurse::simulation::VoxelCell* buffer;
        recurse::EssencePalette* palette;
        int cx, cy, cz;
    };
    std::vector<GenTask> tasks;
    tasks.reserve(chunks.size());

    for (const auto& [cx, cy, cz] : chunks) {
        auto absent = recurse::simulation::addChunkRef(grid.registry(), cx, cy, cz);
        recurse::simulation::transition<recurse::simulation::Absent, recurse::simulation::Generating>(absent,
                                                                                                      grid.registry());
        grid.materializeChunk(cx, cy, cz);
        auto* buf = grid.writeBuffer(cx, cy, cz);
        auto* pal = grid.chunkPalette(cx, cy, cz);
        if (buf)
            tasks.push_back({buf->data(), pal, cx, cy, cz});
    }

    // Phase 1: Parallel generation + essence assignment. Each worker writes
    // exclusively to its chunk's pre-resolved buffer and palette.
    {
        FABRIC_ZONE_SCOPED_N("gen_parallel");
        const auto& mats = fabSim_->materials();
        sched.parallelFor(tasks.size(), [&](size_t idx, size_t /*workerIdx*/) {
            gen.generateToBuffer(tasks[idx].buffer, tasks[idx].cx, tasks[idx].cy, tasks[idx].cz);
            if (tasks[idx].palette) {
                recurse::simulation::assignEssence(tasks[idx].buffer, tasks[idx].cx, tasks[idx].cy, tasks[idx].cz, mats,
                                                   *tasks[idx].palette, 42);
            }
        });
    }

    // Phase 2: Sequential finalization (state transitions, events, neighbor notifications).
    for (const auto& [cx, cy, cz] : chunks) {
        grid.syncChunkBuffers(cx, cy, cz);
        if (auto gen = recurse::simulation::findAs<recurse::simulation::Generating>(grid.registry(), cx, cy, cz))
            recurse::simulation::transition<recurse::simulation::Generating, recurse::simulation::Active>(
                *gen, grid.registry());
        tracker.setState(recurse::simulation::ChunkCoord{cx, cy, cz}, recurse::simulation::ChunkState::Active);

        if (dispatcher_) {
            fabric::Event e(K_VOXEL_CHANGED_EVENT, "VoxelSimulationSystem");
            e.setData("cx", cx);
            e.setData("cy", cy);
            e.setData("cz", cz);
            e.setData("source", static_cast<int>(ChangeSource::Generation));
            dispatcher_->dispatchEvent(e);
        }
    }

    for (const auto& [cx, cy, cz] : chunks) {
        int notifiedCount = 0;
        for (int d = 0; d < 10; ++d) {
            recurse::simulation::ChunkCoord neighbor{cx + K_FACE_DIAGONAL_NEIGHBORS[d][0],
                                                     cy + K_FACE_DIAGONAL_NEIGHBORS[d][1],
                                                     cz + K_FACE_DIAGONAL_NEIGHBORS[d][2]};
            if (fabSim_->grid().hasChunk(neighbor.x, neighbor.y, neighbor.z)) {
                fabSim_->activityTracker().notifyBoundaryChange(neighbor);
                ++notifiedCount;
            }
        }
        if (notifiedCount > 0) {
            FABRIC_LOG_TRACE("Notified {} existing neighbors (face+diagonal) after batch-loading chunk ({},{},{})",
                             notifiedCount, cx, cy, cz);
        }
    }

    FABRIC_LOG_DEBUG("generateChunksBatch: {} chunks generated in parallel", chunks.size());
}

void VoxelSimulationSystem::removeActiveChunk(recurse::simulation::ChunkRef<recurse::simulation::Active> ref) {
    if (!fabSim_)
        return;
    auto& registry = fabSim_->grid().registry();
    auto draining =
        recurse::simulation::transition<recurse::simulation::Active, recurse::simulation::Draining>(ref, registry);
    fabSim_->grid().removeChunk(draining.cx(), draining.cy(), draining.cz());
    fabSim_->activityTracker().remove(fabric::ChunkCoord{draining.cx(), draining.cy(), draining.cz()});
}

void VoxelSimulationSystem::cancelChunk(recurse::simulation::ChunkRef<recurse::simulation::Generating> ref) {
    if (!fabSim_)
        return;
    auto& registry = fabSim_->grid().registry();
    recurse::simulation::cancelAndRemove(ref, registry);
    fabSim_->activityTracker().remove(fabric::ChunkCoord{ref.cx(), ref.cy(), ref.cz()});
}

void VoxelSimulationSystem::removeChunk(int cx, int cy, int cz) {
    if (!fabSim_)
        return;
    auto& registry = fabSim_->grid().registry();
    if (auto active = recurse::simulation::findAs<recurse::simulation::Active>(registry, cx, cy, cz)) {
        removeActiveChunk(*active);
    } else if (auto gen = recurse::simulation::findAs<recurse::simulation::Generating>(registry, cx, cy, cz)) {
        cancelChunk(*gen);
    } else {
        fabSim_->grid().removeChunk(cx, cy, cz);
        fabSim_->activityTracker().remove(recurse::simulation::ChunkCoord{cx, cy, cz});
    }
}

void VoxelSimulationSystem::resetWorld() {
    if (!fabSim_)
        return;

    fabSim_->grid().clear();
    fabSim_->activityTracker().clear();
    fabSim_->resetWorldState();
    lastActiveCount_ = 0;

    FABRIC_LOG_INFO("VoxelSimulationSystem: world reset complete");
}

} // namespace recurse::systems
