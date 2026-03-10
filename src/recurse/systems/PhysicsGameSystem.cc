#include "recurse/systems/PhysicsGameSystem.hh"
#include "recurse/systems/TerrainSystem.hh"
#include "recurse/systems/VoxelSimulationSystem.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/Event.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/utils/Profiler.hh"
#include "recurse/character/VoxelInteraction.hh"
#include "recurse/simulation/ChunkRegistry.hh"

#include <algorithm>
#include <cmath>

namespace recurse::systems {

void PhysicsGameSystem::doInit(fabric::AppContext& ctx) {
    terrain_ = ctx.systemRegistry.get<TerrainSystem>();
    voxelSim_ = ctx.systemRegistry.get<VoxelSimulationSystem>();
    if (voxelSim_)
        scheduler_ = &voxelSim_->scheduler();

    physicsWorld_.init(4096, 0);

    ctx.dispatcher.addEventListener(K_VOXEL_CHANGED_EVENT, [this](fabric::Event& e) {
        int cx = e.getData<int>("cx");
        int cy = e.getData<int>("cy");
        int cz = e.getData<int>("cz");
        dirtyCollisionChunks_.insert({cx, cy, cz});
    });

    ragdoll_.init(&physicsWorld_);

    FABRIC_LOG_INFO("PhysicsGameSystem initialized (scheduler={})", scheduler_ != nullptr);
}

void PhysicsGameSystem::doShutdown() {
    ragdoll_.shutdown();
    physicsWorld_.shutdown();
    voxelSim_ = nullptr;
    scheduler_ = nullptr;
}

void PhysicsGameSystem::fixedUpdate(fabric::AppContext& /*ctx*/, float fixedDt) {
    FABRIC_ZONE_SCOPED_N("physics_step");

    if (!dirtyCollisionChunks_.empty() && voxelSim_) {
        std::vector<recurse::ChunkKey> candidates(dirtyCollisionChunks_.begin(), dirtyCollisionChunks_.end());
        dirtyCollisionChunks_.clear();

        auto& registry = voxelSim_->simulationGrid().registry();
        std::erase_if(candidates, [&registry](const recurse::ChunkKey& k) {
            auto* slot = registry.find(k.cx, k.cy, k.cz);
            return !slot || slot->state != recurse::simulation::ChunkSlotState::Active;
        });

        int pcx = static_cast<int>(std::floor(playerX_ / static_cast<float>(fabric::K_CHUNK_SIZE)));
        int pcy = static_cast<int>(std::floor(playerY_ / static_cast<float>(fabric::K_CHUNK_SIZE)));
        int pcz = static_cast<int>(std::floor(playerZ_ / static_cast<float>(fabric::K_CHUNK_SIZE)));

        std::sort(candidates.begin(), candidates.end(),
                  [pcx, pcy, pcz](const recurse::ChunkKey& a, const recurse::ChunkKey& b) {
                      int da = (a.cx - pcx) * (a.cx - pcx) + (a.cy - pcy) * (a.cy - pcy) + (a.cz - pcz) * (a.cz - pcz);
                      int db = (b.cx - pcx) * (b.cx - pcx) + (b.cy - pcy) * (b.cy - pcy) + (b.cz - pcz) * (b.cz - pcz);
                      return da < db;
                  });

        int limit = std::min(static_cast<int>(candidates.size()), K_COLLISION_BUDGET_PER_FRAME);
        std::vector<recurse::ChunkKey> toRebuild(candidates.begin(), candidates.begin() + limit);

        for (int i = limit; i < static_cast<int>(candidates.size()); ++i)
            dirtyCollisionChunks_.insert(candidates[static_cast<size_t>(i)]);

        if (scheduler_) {
            physicsWorld_.rebuildChunkCollisionBatch(voxelSim_->simulationGrid(), toRebuild, *scheduler_);
        } else {
            for (const auto& key : toRebuild)
                physicsWorld_.rebuildChunkCollision(voxelSim_->simulationGrid(), key.cx, key.cy, key.cz);
        }
    }

    physicsWorld_.step(fixedDt);
}

void PhysicsGameSystem::configureDependencies() {
    after<TerrainSystem>();
    after<VoxelSimulationSystem>();
}

void PhysicsGameSystem::removeDirtyChunk(int cx, int cy, int cz) {
    dirtyCollisionChunks_.erase({cx, cy, cz});
}

void PhysicsGameSystem::setPlayerPosition(float x, float y, float z) {
    playerX_ = x;
    playerY_ = y;
    playerZ_ = z;
}

void PhysicsGameSystem::clearAllCollisions() {
    physicsWorld_.clearChunkBodies();
    FABRIC_LOG_INFO("PhysicsGameSystem: All terrain collision bodies cleared");
}

} // namespace recurse::systems
