#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include "fabric/world/ChunkedGrid.hh"
#include "recurse/world/VoxelRaycast.hh"

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace recurse::simulation {
class SimulationGrid;
} // namespace recurse::simulation

namespace recurse {

// Engine types imported from fabric:: namespace
using fabric::ChunkedGrid;
using fabric::K_CHUNK_SIZE;

class JoltCharacterController;
struct JoltCharacterConfig;

namespace physics {

inline constexpr JPH::ObjectLayer K_LAYER_STATIC = 0;
inline constexpr JPH::ObjectLayer K_LAYER_DYNAMIC = 1;
inline constexpr int K_NUM_OBJECT_LAYERS = 2;

inline constexpr JPH::BroadPhaseLayer K_BP_LAYER_NON_MOVING(0);
inline constexpr JPH::BroadPhaseLayer K_BP_LAYER_MOVING(1);
inline constexpr int K_NUM_BROAD_PHASE_LAYERS = 2;

// 8^3 sub-chunk tiles for fast partial physics rebuild
inline constexpr int K_PHYS_TILE_SIZE = 8;
inline constexpr int K_TILES_PER_AXIS = K_CHUNK_SIZE / K_PHYS_TILE_SIZE;                         // 4
inline constexpr int K_TILES_PER_CHUNK = K_TILES_PER_AXIS * K_TILES_PER_AXIS * K_TILES_PER_AXIS; // 64

class BPLayerInterface final : public JPH::BroadPhaseLayerInterface {
  public:
    uint GetNumBroadPhaseLayers() const override { return K_NUM_BROAD_PHASE_LAYERS; }

    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
        if (inLayer == K_LAYER_STATIC)
            return K_BP_LAYER_NON_MOVING;
        return K_BP_LAYER_MOVING;
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
        if (inLayer == K_BP_LAYER_NON_MOVING)
            return "NON_MOVING";
        if (inLayer == K_BP_LAYER_MOVING)
            return "MOVING";
        return "UNKNOWN";
    }
#endif
};

class ObjectVsBPFilter final : public JPH::ObjectVsBroadPhaseLayerFilter {
  public:
    bool ShouldCollide(JPH::ObjectLayer inLayer, JPH::BroadPhaseLayer inBPLayer) const override {
        if (inLayer == K_LAYER_STATIC)
            return inBPLayer == K_BP_LAYER_MOVING;
        return true;
    }
};

class ObjectPairFilter final : public JPH::ObjectLayerPairFilter {
  public:
    bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::ObjectLayer inLayer2) const override {
        if (inLayer1 == K_LAYER_STATIC && inLayer2 == K_LAYER_STATIC)
            return false;
        return true;
    }
};

} // namespace physics

struct ContactEvent {
    JPH::BodyID bodyA;
    JPH::BodyID bodyB;
};

using ContactCallback = std::function<void(const ContactEvent&)>;

struct BodyHandle {
    JPH::BodyID id;
    bool valid() const { return !id.IsInvalid(); }
};

// Chunk coordinate key for collision shape tracking
struct ChunkKey {
    int cx, cy, cz;
    bool operator==(const ChunkKey& o) const { return cx == o.cx && cy == o.cy && cz == o.cz; }
};

struct ChunkKeyHash {
    size_t operator()(const ChunkKey& k) const {
        size_t h = static_cast<size_t>(k.cx) * 73856093u;
        h ^= static_cast<size_t>(k.cy) * 19349669u;
        h ^= static_cast<size_t>(k.cz) * 83492791u;
        return h;
    }
};

struct Velocity3 {
    float x, y, z;
};

struct Rotation4 {
    float x, y, z, w;
};

struct ConstraintHandle {
    uint32_t id;
    bool valid() const { return id != 0; }
};

class PhysicsWorld {
  public:
    PhysicsWorld();
    ~PhysicsWorld();

    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    void init(uint32_t maxBodies = 4096, int numThreads = 0);
    void shutdown();
    void step(float dt, int collisionSteps = 1);

    BodyHandle createStaticBody(const JPH::Shape* shape, float px, float py, float pz);
    BodyHandle createDynamicBody(const JPH::Shape* shape, float px, float py, float pz, float mass = 1.0f);
    void removeBody(BodyHandle handle);

    void rebuildChunkCollision(const ChunkedGrid<float>& grid, int cx, int cy, int cz, float densityThreshold = 0.5f);
    void rebuildChunkCollision(const recurse::simulation::SimulationGrid& grid, int cx, int cy, int cz);
    void removeChunkCollision(int cx, int cy, int cz);
    void clearChunkBodies(); // Remove all chunk collision bodies (for world reset)
    uint32_t chunkCollisionShapeCount(int cx, int cy, int cz) const;

    void setContactCallback(ContactCallback cb);

    void applyForce(BodyHandle handle, float fx, float fy, float fz);
    void applyImpulse(BodyHandle handle, float ix, float iy, float iz);
    void applyTorque(BodyHandle handle, float tx, float ty, float tz);
    void setLinearVelocity(BodyHandle handle, float vx, float vy, float vz);
    Velocity3 getLinearVelocity(BodyHandle handle);

    void setFriction(BodyHandle handle, float friction);
    void setRestitution(BodyHandle handle, float restitution);
    void setLinearDamping(BodyHandle handle, float damping);
    void setAngularDamping(BodyHandle handle, float damping);

    Velocity3 getBodyPosition(BodyHandle handle);
    Rotation4 getBodyRotation(BodyHandle handle);

    BodyHandle createDebris(const JPH::Shape* shape, float px, float py, float pz, float vx, float vy, float vz,
                            float lifetime);
    uint32_t debrisCount() const;

    ConstraintHandle createFixedConstraint(BodyHandle a, BodyHandle b);
    void removeConstraint(ConstraintHandle handle);

    JPH::PhysicsSystem* joltSystem();
    bool initialized() const;

    // Temp allocator access (required for CharacterVirtual updates)
    JPH::TempAllocator* tempAllocator();

    // CharacterVirtual management
    JoltCharacterController* createCharacter(const JoltCharacterConfig& config);
    void destroyCharacter(JoltCharacterController* character);

    // CCD: grid-based raycast for fast projectiles (DDA)
    std::optional<VoxelHit> castProjectileRay(const ChunkedGrid<float>& grid, float ox, float oy, float oz, float dx,
                                              float dy, float dz, float maxDistance, float densityThreshold = 0.5f);

    // CCD: swept AABB for entity-to-entity continuous collision
    bool sweptAABBIntersect(float ax1, float ay1, float az1, float ax2, float ay2, float az2, float vx, float vy,
                            float vz, float dt, float bx1, float by1, float bz1, float bx2, float by2, float bz2,
                            float* outT = nullptr);

  private:
    class ContactListenerImpl;

    bool initialized_ = false;
    std::unique_ptr<JPH::TempAllocatorImpl> tempAllocator_;
    std::unique_ptr<JPH::JobSystemThreadPool> jobSystem_;
    std::unique_ptr<JPH::PhysicsSystem> physicsSystem_;
    std::unique_ptr<ContactListenerImpl> contactListener_;

    physics::BPLayerInterface bpLayerInterface_;
    physics::ObjectVsBPFilter objectVsBPFilter_;
    physics::ObjectPairFilter objectPairFilter_;

    // Per-chunk collision bodies (terrain)
    std::unordered_map<ChunkKey, std::vector<JPH::BodyID>, ChunkKeyHash> chunkBodies_;

    // All user-created bodies (static + dynamic via public API)
    std::vector<JPH::BodyID> userBodies_;

    struct DebrisEntry {
        JPH::BodyID bodyId;
        float lifetime;
        float elapsed;
    };
    std::vector<DebrisEntry> debris_;

    uint32_t nextConstraintId_ = 1;
    std::unordered_map<uint32_t, JPH::Constraint*> constraints_;

    // CharacterVirtual controllers
    std::vector<std::unique_ptr<JoltCharacterController>> characters_;
};

} // namespace recurse
