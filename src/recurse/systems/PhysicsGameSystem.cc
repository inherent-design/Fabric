#include "recurse/systems/PhysicsGameSystem.hh"
#include "recurse/systems/TerrainSystem.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/Event.hh"
#include "fabric/core/Log.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/utils/Profiler.hh"
#include "recurse/gameplay/VoxelInteraction.hh"

namespace recurse::systems {

void PhysicsGameSystem::init(fabric::AppContext& ctx) {
    terrain_ = ctx.systemRegistry.get<TerrainSystem>();

    physicsWorld_.init(4096, 0);

    // Rebuild chunk collision geometry when voxel data changes
    ctx.dispatcher.addEventListener(kVoxelChangedEvent, [this](fabric::Event& e) {
        int cx = e.getData<int>("cx");
        int cy = e.getData<int>("cy");
        int cz = e.getData<int>("cz");
        physicsWorld_.rebuildChunkCollision(terrain_->densityGrid(), cx, cy, cz);
    });

    ragdoll_.init(&physicsWorld_);

    FABRIC_LOG_INFO("PhysicsGameSystem initialized");
}

void PhysicsGameSystem::shutdown() {
    ragdoll_.shutdown();
    physicsWorld_.shutdown();
}

void PhysicsGameSystem::fixedUpdate(fabric::AppContext& /*ctx*/, float fixedDt) {
    FABRIC_ZONE_SCOPED_N("physics_step");
    physicsWorld_.step(fixedDt);
}

void PhysicsGameSystem::configureDependencies() {
    after<TerrainSystem>();
}

} // namespace recurse::systems
