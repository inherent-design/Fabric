#include "recurse/systems/PhysicsGameSystem.hh"
#include "recurse/systems/TerrainSystem.hh"
#include "recurse/systems/VoxelSimulationSystem.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/Event.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/utils/Profiler.hh"
#include "recurse/character/VoxelInteraction.hh"

namespace recurse::systems {

void PhysicsGameSystem::doInit(fabric::AppContext& ctx) {
    terrain_ = ctx.systemRegistry.get<TerrainSystem>();
    voxelSim_ = ctx.systemRegistry.get<VoxelSimulationSystem>();

    physicsWorld_.init(4096, 0);

    ctx.dispatcher.addEventListener(K_VOXEL_CHANGED_EVENT, [this](fabric::Event& e) {
        int cx = e.getData<int>("cx");
        int cy = e.getData<int>("cy");
        int cz = e.getData<int>("cz");
        dirtyCollisionChunks_.insert({cx, cy, cz});
    });

    ragdoll_.init(&physicsWorld_);

    FABRIC_LOG_INFO("PhysicsGameSystem initialized");
}

void PhysicsGameSystem::doShutdown() {
    ragdoll_.shutdown();
    physicsWorld_.shutdown();
    voxelSim_ = nullptr;
}

void PhysicsGameSystem::fixedUpdate(fabric::AppContext& /*ctx*/, float fixedDt) {
    FABRIC_ZONE_SCOPED_N("physics_step");

    if (!dirtyCollisionChunks_.empty() && voxelSim_) {
        for (const auto& key : dirtyCollisionChunks_) {
            physicsWorld_.rebuildChunkCollision(voxelSim_->simulationGrid(), key.cx, key.cy, key.cz);
        }
        dirtyCollisionChunks_.clear();
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

void PhysicsGameSystem::clearAllCollisions() {
    physicsWorld_.clearChunkBodies();
    FABRIC_LOG_INFO("PhysicsGameSystem: All terrain collision bodies cleared");
}

} // namespace recurse::systems
