#include "recurse/systems/ChunkPipelineSystem.hh"
#include "recurse/systems/CharacterMovementSystem.hh"
#include "recurse/systems/PhysicsGameSystem.hh"
#include "recurse/systems/TerrainSystem.hh"
#include "recurse/systems/VoxelSimulationSystem.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/ECS.hh"
#include "fabric/core/Event.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/utils/Profiler.hh"

#include <cmath>

namespace {
constexpr float kSpawnX = 16.0f;
constexpr float kSpawnY = 48.0f;
constexpr float kSpawnZ = 16.0f;
} // namespace

namespace recurse::systems {

ChunkPipelineSystem::~ChunkPipelineSystem() = default;

void ChunkPipelineSystem::init(fabric::AppContext& ctx) {
    terrain_ = ctx.systemRegistry.get<TerrainSystem>();
    simSystem_ = ctx.systemRegistry.get<VoxelSimulationSystem>();
    physics_ = ctx.systemRegistry.get<PhysicsGameSystem>();
    charMovement_ = ctx.systemRegistry.get<CharacterMovementSystem>();

    // Chunk streaming
    StreamingConfig streamConfig;
    streamConfig.baseRadius = 3;
    streamConfig.maxRadius = 5;
    streamConfig.maxLoadsPerTick = 2;
    streamConfig.maxUnloadsPerTick = 4;
    streaming_ = std::make_unique<ChunkStreamingManager>(streamConfig);

    // Initial chunk load (ECS entities only, no meshing)
    {
        FABRIC_ZONE_SCOPED_N("initial_terrain");
        auto& ecsWorld = ctx.world;
        auto initLoad = streaming_->update(kSpawnX, kSpawnY, kSpawnZ, 0.0f);

        for (const auto& coord : initLoad.toLoad) {
            auto ent = ecsWorld.get().entity().add<fabric::SceneEntity>().set<fabric::BoundingBox>(
                {static_cast<float>(coord.cx * kChunkSize), static_cast<float>(coord.cy * kChunkSize),
                 static_cast<float>(coord.cz * kChunkSize), static_cast<float>((coord.cx + 1) * kChunkSize),
                 static_cast<float>((coord.cy + 1) * kChunkSize), static_cast<float>((coord.cz + 1) * kChunkSize)});
            chunkEntities_[coord] = ent;
        }

        FABRIC_LOG_INFO("Initial terrain: {} chunks loaded (meshing deferred to simulation layer)",
                        initLoad.toLoad.size());
    }
}

void ChunkPipelineSystem::shutdown() {
    for (auto& [_, entity] : chunkEntities_) {
        entity.destruct();
    }
    chunkEntities_.clear();
}

void ChunkPipelineSystem::fixedUpdate(fabric::AppContext& ctx, float /*fixedDt*/) {
    FABRIC_ZONE_SCOPED_N("chunk_pipeline");
    auto& ecsWorld = ctx.world;

    // Streaming: load/unload chunks around player position
    float px = kSpawnX, py = kSpawnY, pz = kSpawnZ;
    float speed = 0.0f;

    if (charMovement_) {
        const auto& pos = charMovement_->playerPosition();
        px = pos.x;
        py = pos.y;
        pz = pos.z;
        const auto& vel = charMovement_->playerVelocity();
        speed = std::sqrt(vel.x * vel.x + vel.y * vel.y + vel.z * vel.z);
    }

    auto streamUpdate = streaming_->update(px, py, pz, speed);

    for (const auto& coord : streamUpdate.toLoad) {
        if (chunkEntities_.find(coord) == chunkEntities_.end()) {
            // Generate terrain into simulation grid
            if (simSystem_)
                simSystem_->generateChunk(coord.cx, coord.cy, coord.cz);

            auto ent = ecsWorld.get().entity().add<fabric::SceneEntity>().set<fabric::BoundingBox>(
                {static_cast<float>(coord.cx * kChunkSize), static_cast<float>(coord.cy * kChunkSize),
                 static_cast<float>(coord.cz * kChunkSize), static_cast<float>((coord.cx + 1) * kChunkSize),
                 static_cast<float>((coord.cy + 1) * kChunkSize), static_cast<float>((coord.cz + 1) * kChunkSize)});
            chunkEntities_[coord] = ent;
        }
    }
    for (const auto& coord : streamUpdate.toUnload) {
        // Remove from simulation grid
        if (simSystem_)
            simSystem_->removeChunk(coord.cx, coord.cy, coord.cz);

        if (physics_)
            physics_->physicsWorld().removeChunkCollision(coord.cx, coord.cy, coord.cz);

        if (auto it = chunkEntities_.find(coord); it != chunkEntities_.end()) {
            it->second.destruct();
            chunkEntities_.erase(it);
        }
    }
}

void ChunkPipelineSystem::configureDependencies() {
    after<TerrainSystem>();
    after<VoxelSimulationSystem>();
    after<PhysicsGameSystem>();
    after<CharacterMovementSystem>();
}

} // namespace recurse::systems
