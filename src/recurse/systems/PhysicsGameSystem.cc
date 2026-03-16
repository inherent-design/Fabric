#include "recurse/systems/PhysicsGameSystem.hh"
#include "recurse/systems/TerrainSystem.hh"
#include "recurse/systems/VoxelSimulationSystem.hh"

#include "recurse/config/RecurseConfig.hh"

#include "fabric/core/AppContext.hh"
#include "fabric/core/Event.hh"
#include "fabric/core/SystemRegistry.hh"
#include "fabric/core/WorldLifecycle.hh"
#include "fabric/log/Log.hh"
#include "fabric/platform/ConfigManager.hh"
#include "fabric/utils/Profiler.hh"
#include "recurse/character/VoxelInteraction.hh"
#include "recurse/simulation/ChunkRegistry.hh"
#include "recurse/simulation/ChunkState.hh"

#include <algorithm>
#include <cmath>

namespace recurse::systems {

void PhysicsGameSystem::doInit(fabric::AppContext& ctx) {
    if (auto* wl = ctx.worldLifecycle) {
        wl->registerParticipant([this]() { onWorldBegin(); }, [this]() { onWorldEnd(); });
    }
    terrain_ = ctx.systemRegistry.get<TerrainSystem>();
    voxelSim_ = ctx.systemRegistry.get<VoxelSimulationSystem>();
    if (voxelSim_)
        scheduler_ = &voxelSim_->scheduler();

    physicsWorld_.init(4096, 0);

    dispatcher_ = &ctx.dispatcher;
    voxelChangedListenerId_ = ctx.dispatcher.addEventListener(K_VOXEL_CHANGED_EVENT, [this](fabric::Event& e) {
        int cx = e.getData<int>("cx");
        int cy = e.getData<int>("cy");
        int cz = e.getData<int>("cz");
        dirtyCollisionChunks_.insert({cx, cy, cz});
    });

    ragdoll_.init(&physicsWorld_);

    collisionBudget_ =
        ctx.configManager.get<int>("physics.collision_budget", recurse::RecurseConfig::K_DEFAULT_COLLISION_BUDGET);
    collisionRadius_ =
        ctx.configManager.get<int>("physics.collision_radius", recurse::RecurseConfig::K_DEFAULT_COLLISION_RADIUS);

    FABRIC_LOG_INFO("PhysicsGameSystem initialized (scheduler={})", scheduler_ != nullptr);
}

void PhysicsGameSystem::doShutdown() {
    if (dispatcher_ && !voxelChangedListenerId_.empty())
        dispatcher_->removeEventListener(K_VOXEL_CHANGED_EVENT, voxelChangedListenerId_);

    ragdoll_.shutdown();
    physicsWorld_.shutdown();
    voxelSim_ = nullptr;
    scheduler_ = nullptr;
    dispatcher_ = nullptr;
}

void PhysicsGameSystem::fixedUpdate(fabric::AppContext& /*ctx*/, float fixedDt) {
    FABRIC_ZONE_SCOPED_N("physics_step");

    std::vector<recurse::CollisionCenter> currentFocalCoords;
    currentFocalCoords.reserve(focalPoints_.size());
    for (const auto& fp : focalPoints_) {
        currentFocalCoords.push_back(
            {static_cast<int>(std::floor(fp.x / static_cast<float>(recurse::simulation::K_CHUNK_SIZE))),
             static_cast<int>(std::floor(fp.y / static_cast<float>(recurse::simulation::K_CHUNK_SIZE))),
             static_cast<int>(std::floor(fp.z / static_cast<float>(recurse::simulation::K_CHUNK_SIZE))), fp.radius});
    }

    if (!dirtyCollisionChunks_.empty() && voxelSim_ && !currentFocalCoords.empty()) {
        std::vector<recurse::ChunkCoord> candidates(dirtyCollisionChunks_.begin(), dirtyCollisionChunks_.end());
        dirtyCollisionChunks_.clear();

        auto& registry = voxelSim_->simulationGrid().registry();
        std::erase_if(candidates, [&registry](const recurse::ChunkCoord& k) {
            return !recurse::simulation::findAs<recurse::simulation::Active>(registry, k.x, k.y, k.z);
        });

        // Drop chunks beyond ALL focal points' collision radii
        std::erase_if(candidates, [&currentFocalCoords](const recurse::ChunkCoord& k) {
            for (const auto& c : currentFocalCoords) {
                int dx = k.x - c.cx, dy = k.y - c.cy, dz = k.z - c.cz;
                if (dx * dx + dy * dy + dz * dz <= c.radius * c.radius)
                    return false;
            }
            return true;
        });

        auto minDist = [&currentFocalCoords](const recurse::ChunkCoord& k) {
            int best = INT_MAX;
            for (const auto& c : currentFocalCoords) {
                int dx = k.x - c.cx, dy = k.y - c.cy, dz = k.z - c.cz;
                int d = dx * dx + dy * dy + dz * dz;
                if (d < best)
                    best = d;
            }
            return best;
        };
        std::sort(candidates.begin(), candidates.end(),
                  [&](const recurse::ChunkCoord& a, const recurse::ChunkCoord& b) { return minDist(a) < minDist(b); });

        int limit = std::min(static_cast<int>(candidates.size()), collisionBudget_);
        std::vector<recurse::ChunkCoord> toRebuild(candidates.begin(), candidates.begin() + limit);

        for (int i = limit; i < static_cast<int>(candidates.size()); ++i)
            dirtyCollisionChunks_.insert(candidates[static_cast<size_t>(i)]);

        if (scheduler_) {
            physicsWorld_.rebuildChunkCollisionBatch(voxelSim_->simulationGrid(), toRebuild, *scheduler_);
        } else {
            for (const auto& key : toRebuild)
                physicsWorld_.rebuildChunkCollision(voxelSim_->simulationGrid(), key.x, key.y, key.z);
        }
    }

    // Proactive cleanup + re-dirty when focal point set changes
    if (!currentFocalCoords.empty()) {
        std::sort(currentFocalCoords.begin(), currentFocalCoords.end());
        if (currentFocalCoords != lastFocalChunkCoords_) {
            lastFocalChunkCoords_ = currentFocalCoords;
            physicsWorld_.removeCollisionBeyondAll(currentFocalCoords);

            if (voxelSim_) {
                auto& registry = voxelSim_->simulationGrid().registry();
                for (const auto& c : currentFocalCoords) {
                    int r = c.radius;
                    for (int dz = -r; dz <= r; ++dz) {
                        for (int dy = -r; dy <= r; ++dy) {
                            for (int dx = -r; dx <= r; ++dx) {
                                if (dx * dx + dy * dy + dz * dz > r * r)
                                    continue;
                                int ccx = c.cx + dx, ccy = c.cy + dy, ccz = c.cz + dz;
                                if (recurse::simulation::findAs<recurse::simulation::Active>(registry, ccx, ccy, ccz) &&
                                    !physicsWorld_.hasChunkCollision(ccx, ccy, ccz))
                                    dirtyCollisionChunks_.insert({ccx, ccy, ccz});
                            }
                        }
                    }
                }
            }
        }
    }

    physicsWorld_.step(fixedDt);
}

void PhysicsGameSystem::onWorldBegin() {
    dirtyCollisionChunks_.clear();
}

void PhysicsGameSystem::onWorldEnd() {
    dirtyCollisionChunks_.clear();
    focalPoints_.clear();
    lastFocalChunkCoords_.clear();
    physicsWorld_.resetWorldState();
    ragdoll_.clear();
}

void PhysicsGameSystem::configureDependencies() {
    after<TerrainSystem>();
    after<VoxelSimulationSystem>();
}

void PhysicsGameSystem::removeDirtyChunk(int cx, int cy, int cz) {
    dirtyCollisionChunks_.erase({cx, cy, cz});
}

void PhysicsGameSystem::setFocalPoints(const std::vector<recurse::FocalPoint>& points) {
    focalPoints_ = points;
}

void PhysicsGameSystem::clearAllCollisions() {
    physicsWorld_.clearChunkBodies();
    FABRIC_LOG_INFO("PhysicsGameSystem: All terrain collision bodies cleared");
}

} // namespace recurse::systems
