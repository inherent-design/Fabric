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

    std::vector<recurse::CollisionCenter> currentFocalCoords;
    currentFocalCoords.reserve(focalPoints_.size());
    for (const auto& fp : focalPoints_) {
        currentFocalCoords.push_back({static_cast<int>(std::floor(fp.x / static_cast<float>(fabric::K_CHUNK_SIZE))),
                                      static_cast<int>(std::floor(fp.y / static_cast<float>(fabric::K_CHUNK_SIZE))),
                                      static_cast<int>(std::floor(fp.z / static_cast<float>(fabric::K_CHUNK_SIZE))),
                                      fp.radius});
    }

    if (!dirtyCollisionChunks_.empty() && voxelSim_ && !currentFocalCoords.empty()) {
        std::vector<recurse::ChunkKey> candidates(dirtyCollisionChunks_.begin(), dirtyCollisionChunks_.end());
        dirtyCollisionChunks_.clear();

        auto& registry = voxelSim_->simulationGrid().registry();
        std::erase_if(candidates, [&registry](const recurse::ChunkKey& k) {
            auto* slot = registry.find(k.cx, k.cy, k.cz);
            return !slot || slot->state != recurse::simulation::ChunkSlotState::Active;
        });

        // Drop chunks beyond ALL focal points' collision radii
        std::erase_if(candidates, [&currentFocalCoords](const recurse::ChunkKey& k) {
            for (const auto& c : currentFocalCoords) {
                int dx = k.cx - c.cx, dy = k.cy - c.cy, dz = k.cz - c.cz;
                if (dx * dx + dy * dy + dz * dz <= c.radius * c.radius)
                    return false;
            }
            return true;
        });

        auto minDist = [&currentFocalCoords](const recurse::ChunkKey& k) {
            int best = INT_MAX;
            for (const auto& c : currentFocalCoords) {
                int dx = k.cx - c.cx, dy = k.cy - c.cy, dz = k.cz - c.cz;
                int d = dx * dx + dy * dy + dz * dz;
                if (d < best)
                    best = d;
            }
            return best;
        };
        std::sort(candidates.begin(), candidates.end(),
                  [&](const recurse::ChunkKey& a, const recurse::ChunkKey& b) { return minDist(a) < minDist(b); });

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
                                auto* slot = registry.find(ccx, ccy, ccz);
                                if (slot && slot->state == recurse::simulation::ChunkSlotState::Active &&
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
