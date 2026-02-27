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

#include "fabric/core/ChunkedGrid.hh"

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace fabric {

namespace physics {

inline constexpr JPH::ObjectLayer kLayerStatic = 0;
inline constexpr JPH::ObjectLayer kLayerDynamic = 1;
inline constexpr int kNumObjectLayers = 2;

inline constexpr JPH::BroadPhaseLayer kBPLayerNonMoving(0);
inline constexpr JPH::BroadPhaseLayer kBPLayerMoving(1);
inline constexpr int kNumBroadPhaseLayers = 2;

// 8^3 sub-chunk tiles for fast partial physics rebuild
inline constexpr int kPhysTileSize = 8;
inline constexpr int kTilesPerAxis = kChunkSize / kPhysTileSize;                     // 4
inline constexpr int kTilesPerChunk = kTilesPerAxis * kTilesPerAxis * kTilesPerAxis; // 64

class BPLayerInterface final : public JPH::BroadPhaseLayerInterface {
  public:
    uint GetNumBroadPhaseLayers() const override { return kNumBroadPhaseLayers; }

    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
        if (inLayer == kLayerStatic)
            return kBPLayerNonMoving;
        return kBPLayerMoving;
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
        if (inLayer == kBPLayerNonMoving)
            return "NON_MOVING";
        if (inLayer == kBPLayerMoving)
            return "MOVING";
        return "UNKNOWN";
    }
#endif
};

class ObjectVsBPFilter final : public JPH::ObjectVsBroadPhaseLayerFilter {
  public:
    bool ShouldCollide(JPH::ObjectLayer inLayer, JPH::BroadPhaseLayer inBPLayer) const override {
        if (inLayer == kLayerStatic)
            return inBPLayer == kBPLayerMoving;
        return true;
    }
};

class ObjectPairFilter final : public JPH::ObjectLayerPairFilter {
  public:
    bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::ObjectLayer inLayer2) const override {
        if (inLayer1 == kLayerStatic && inLayer2 == kLayerStatic)
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
    void removeChunkCollision(int cx, int cy, int cz);
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
};

} // namespace fabric
