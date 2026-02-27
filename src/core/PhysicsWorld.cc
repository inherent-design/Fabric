#include "fabric/core/PhysicsWorld.hh"

#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <cmath>
#include <mutex>

namespace fabric {

// Contact listener forwards collision events to user callback
class PhysicsWorld::ContactListenerImpl final : public JPH::ContactListener {
  public:
    JPH::ValidateResult OnContactValidate([[maybe_unused]] const JPH::Body& inBody1,
                                          [[maybe_unused]] const JPH::Body& inBody2,
                                          [[maybe_unused]] JPH::RVec3Arg inBaseOffset,
                                          [[maybe_unused]] const JPH::CollideShapeResult& inCollisionResult) override {
        return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
    }

    void OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2,
                        [[maybe_unused]] const JPH::ContactManifold& inManifold,
                        [[maybe_unused]] JPH::ContactSettings& ioSettings) override {
        if (callback_) {
            ContactEvent ev{inBody1.GetID(), inBody2.GetID()};
            callback_(ev);
        }
    }

    void OnContactPersisted([[maybe_unused]] const JPH::Body& inBody1, [[maybe_unused]] const JPH::Body& inBody2,
                            [[maybe_unused]] const JPH::ContactManifold& inManifold,
                            [[maybe_unused]] JPH::ContactSettings& ioSettings) override {}

    void OnContactRemoved([[maybe_unused]] const JPH::SubShapeIDPair& inSubShapePair) override {}

    void setCallback(ContactCallback cb) { callback_ = std::move(cb); }

  private:
    ContactCallback callback_;
};

// Track whether Jolt global state has been initialized in this process
static bool sJoltGlobalInit = false;

static void ensureJoltGlobalInit() {
    if (sJoltGlobalInit)
        return;
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();
    sJoltGlobalInit = true;
}

PhysicsWorld::PhysicsWorld() = default;

PhysicsWorld::~PhysicsWorld() {
    if (initialized_)
        shutdown();
}

void PhysicsWorld::init(uint32_t maxBodies, int numThreads) {
    if (initialized_)
        return;

    ensureJoltGlobalInit();

    // 10 MB temp allocator for Jolt's per-frame scratch memory
    tempAllocator_ = std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);

    // Thread pool: 0 means auto-detect, but use at least 1
    int threads = (numThreads <= 0) ? 1 : numThreads;
    jobSystem_ = std::make_unique<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, threads);

    physicsSystem_ = std::make_unique<JPH::PhysicsSystem>();
    physicsSystem_->Init(maxBodies,
                         0,             // auto body mutexes
                         maxBodies * 2, // max body pairs
                         maxBodies,     // max contact constraints
                         bpLayerInterface_, objectVsBPFilter_, objectPairFilter_);

    contactListener_ = std::make_unique<ContactListenerImpl>();
    physicsSystem_->SetContactListener(contactListener_.get());

    initialized_ = true;
}

void PhysicsWorld::shutdown() {
    if (!initialized_)
        return;

    // Remove constraints first (they reference bodies)
    for (auto& [id, constraint] : constraints_)
        physicsSystem_->RemoveConstraint(constraint);
    constraints_.clear();

    // Remove debris bodies
    {
        auto& bi = physicsSystem_->GetBodyInterface();
        for (auto& entry : debris_) {
            bi.RemoveBody(entry.bodyId);
            bi.DestroyBody(entry.bodyId);
        }
    }
    debris_.clear();

    // Remove all chunk collision bodies
    for (auto& [key, bodies] : chunkBodies_) {
        auto& bi = physicsSystem_->GetBodyInterface();
        for (auto& bodyId : bodies) {
            bi.RemoveBody(bodyId);
            bi.DestroyBody(bodyId);
        }
    }
    chunkBodies_.clear();

    // Remove user-created bodies
    {
        auto& bi = physicsSystem_->GetBodyInterface();
        for (auto& bodyId : userBodies_) {
            bi.RemoveBody(bodyId);
            bi.DestroyBody(bodyId);
        }
    }
    userBodies_.clear();

    physicsSystem_.reset();
    contactListener_.reset();
    jobSystem_.reset();
    tempAllocator_.reset();

    initialized_ = false;
}

void PhysicsWorld::step(float dt, int collisionSteps) {
    if (!initialized_ || dt <= 0.0f)
        return;
    physicsSystem_->Update(dt, collisionSteps, tempAllocator_.get(), jobSystem_.get());

    // Expire debris past their lifetime
    auto& bi = physicsSystem_->GetBodyInterface();
    auto dIt = debris_.begin();
    while (dIt != debris_.end()) {
        dIt->elapsed += dt;
        if (dIt->elapsed >= dIt->lifetime) {
            bi.RemoveBody(dIt->bodyId);
            bi.DestroyBody(dIt->bodyId);
            dIt = debris_.erase(dIt);
        } else {
            ++dIt;
        }
    }
}

BodyHandle PhysicsWorld::createStaticBody(const JPH::Shape* shape, float px, float py, float pz) {
    if (!initialized_ || shape == nullptr)
        return BodyHandle{JPH::BodyID()};

    JPH::BodyCreationSettings settings(shape, JPH::RVec3(px, py, pz), JPH::Quat::sIdentity(), JPH::EMotionType::Static,
                                       physics::kLayerStatic);

    auto& bi = physicsSystem_->GetBodyInterface();
    JPH::Body* body = bi.CreateBody(settings);
    if (body == nullptr)
        return BodyHandle{JPH::BodyID()};

    bi.AddBody(body->GetID(), JPH::EActivation::DontActivate);
    userBodies_.push_back(body->GetID());
    return BodyHandle{body->GetID()};
}

BodyHandle PhysicsWorld::createDynamicBody(const JPH::Shape* shape, float px, float py, float pz, float mass) {
    if (!initialized_ || shape == nullptr)
        return BodyHandle{JPH::BodyID()};

    JPH::BodyCreationSettings settings(shape, JPH::RVec3(px, py, pz), JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic,
                                       physics::kLayerDynamic);

    settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
    settings.mMassPropertiesOverride.mMass = mass;

    auto& bi = physicsSystem_->GetBodyInterface();
    JPH::Body* body = bi.CreateBody(settings);
    if (body == nullptr)
        return BodyHandle{JPH::BodyID()};

    bi.AddBody(body->GetID(), JPH::EActivation::Activate);
    userBodies_.push_back(body->GetID());
    return BodyHandle{body->GetID()};
}

void PhysicsWorld::removeBody(BodyHandle handle) {
    if (!initialized_ || !handle.valid())
        return;

    auto& bi = physicsSystem_->GetBodyInterface();
    bi.RemoveBody(handle.id);
    bi.DestroyBody(handle.id);

    // Remove from user body tracking if present
    auto it =
        std::find_if(userBodies_.begin(), userBodies_.end(), [&](const JPH::BodyID& id) { return id == handle.id; });
    if (it != userBodies_.end())
        userBodies_.erase(it);
}

void PhysicsWorld::rebuildChunkCollision(const ChunkedGrid<float>& grid, int cx, int cy, int cz,
                                         float densityThreshold) {
    if (!initialized_)
        return;

    // Remove existing collision for this chunk
    removeChunkCollision(cx, cy, cz);

    // Iterate 8^3 sub-tiles within the 32^3 chunk
    int baseX = cx * kChunkSize;
    int baseY = cy * kChunkSize;
    int baseZ = cz * kChunkSize;

    ChunkKey key{cx, cy, cz};
    auto& bodies = chunkBodies_[key];

    for (int tz = 0; tz < physics::kTilesPerAxis; ++tz) {
        for (int ty = 0; ty < physics::kTilesPerAxis; ++ty) {
            for (int tx = 0; tx < physics::kTilesPerAxis; ++tx) {
                int tileBaseX = baseX + tx * physics::kPhysTileSize;
                int tileBaseY = baseY + ty * physics::kPhysTileSize;
                int tileBaseZ = baseZ + tz * physics::kPhysTileSize;

                JPH::StaticCompoundShapeSettings compound;
                bool hasSolid = false;

                // Scan voxels in this 8^3 tile
                for (int lz = 0; lz < physics::kPhysTileSize; ++lz) {
                    for (int ly = 0; ly < physics::kPhysTileSize; ++ly) {
                        for (int lx = 0; lx < physics::kPhysTileSize; ++lx) {
                            int wx = tileBaseX + lx;
                            int wy = tileBaseY + ly;
                            int wz = tileBaseZ + lz;

                            float density = grid.get(wx, wy, wz);
                            if (density < densityThreshold)
                                continue;

                            // Each solid voxel becomes a unit box (1x1x1) at its center
                            JPH::Vec3 halfExtent(0.5f, 0.5f, 0.5f);
                            auto boxSettings = new JPH::BoxShapeSettings(halfExtent, 0.0f);

                            // Position relative to tile origin
                            JPH::Vec3 localPos(static_cast<float>(lx) + 0.5f, static_cast<float>(ly) + 0.5f,
                                               static_cast<float>(lz) + 0.5f);
                            compound.AddShape(localPos, JPH::Quat::sIdentity(), boxSettings);
                            hasSolid = true;
                        }
                    }
                }

                if (!hasSolid)
                    continue;

                auto result = compound.Create(*tempAllocator_);
                if (!result.IsValid())
                    continue;

                JPH::BodyCreationSettings bodySettings(
                    result.Get(),
                    JPH::RVec3(static_cast<float>(tileBaseX), static_cast<float>(tileBaseY),
                               static_cast<float>(tileBaseZ)),
                    JPH::Quat::sIdentity(), JPH::EMotionType::Static, physics::kLayerStatic);

                auto& bi = physicsSystem_->GetBodyInterface();
                JPH::Body* body = bi.CreateBody(bodySettings);
                if (body == nullptr)
                    continue;

                bi.AddBody(body->GetID(), JPH::EActivation::DontActivate);
                bodies.push_back(body->GetID());
            }
        }
    }

    // Clean up empty entry
    if (bodies.empty())
        chunkBodies_.erase(key);
}

void PhysicsWorld::removeChunkCollision(int cx, int cy, int cz) {
    if (!initialized_)
        return;

    ChunkKey key{cx, cy, cz};
    auto it = chunkBodies_.find(key);
    if (it == chunkBodies_.end())
        return;

    auto& bi = physicsSystem_->GetBodyInterface();
    for (auto& bodyId : it->second) {
        bi.RemoveBody(bodyId);
        bi.DestroyBody(bodyId);
    }
    chunkBodies_.erase(it);
}

void PhysicsWorld::setContactCallback(ContactCallback cb) {
    if (contactListener_)
        contactListener_->setCallback(std::move(cb));
}

JPH::PhysicsSystem* PhysicsWorld::joltSystem() {
    return physicsSystem_.get();
}

bool PhysicsWorld::initialized() const {
    return initialized_;
}

void PhysicsWorld::applyForce(BodyHandle handle, float fx, float fy, float fz) {
    if (!initialized_ || !handle.valid())
        return;
    physicsSystem_->GetBodyInterface().AddForce(handle.id, JPH::Vec3(fx, fy, fz));
}

void PhysicsWorld::applyImpulse(BodyHandle handle, float ix, float iy, float iz) {
    if (!initialized_ || !handle.valid())
        return;
    physicsSystem_->GetBodyInterface().AddImpulse(handle.id, JPH::Vec3(ix, iy, iz));
}

void PhysicsWorld::applyTorque(BodyHandle handle, float tx, float ty, float tz) {
    if (!initialized_ || !handle.valid())
        return;
    physicsSystem_->GetBodyInterface().AddTorque(handle.id, JPH::Vec3(tx, ty, tz));
}

void PhysicsWorld::setLinearVelocity(BodyHandle handle, float vx, float vy, float vz) {
    if (!initialized_ || !handle.valid())
        return;
    physicsSystem_->GetBodyInterface().SetLinearVelocity(handle.id, JPH::Vec3(vx, vy, vz));
}

Velocity3 PhysicsWorld::getLinearVelocity(BodyHandle handle) {
    if (!initialized_ || !handle.valid())
        return {0.0f, 0.0f, 0.0f};
    auto v = physicsSystem_->GetBodyInterface().GetLinearVelocity(handle.id);
    return {v.GetX(), v.GetY(), v.GetZ()};
}

void PhysicsWorld::setFriction(BodyHandle handle, float friction) {
    if (!initialized_ || !handle.valid())
        return;
    physicsSystem_->GetBodyInterface().SetFriction(handle.id, friction);
}

void PhysicsWorld::setRestitution(BodyHandle handle, float restitution) {
    if (!initialized_ || !handle.valid())
        return;
    physicsSystem_->GetBodyInterface().SetRestitution(handle.id, restitution);
}

void PhysicsWorld::setLinearDamping(BodyHandle handle, float damping) {
    if (!initialized_ || !handle.valid())
        return;
    JPH::BodyLockWrite lock(physicsSystem_->GetBodyLockInterface(), handle.id);
    if (lock.Succeeded()) {
        auto* mp = lock.GetBody().GetMotionProperties();
        if (mp != nullptr)
            mp->SetLinearDamping(damping);
    }
}

void PhysicsWorld::setAngularDamping(BodyHandle handle, float damping) {
    if (!initialized_ || !handle.valid())
        return;
    JPH::BodyLockWrite lock(physicsSystem_->GetBodyLockInterface(), handle.id);
    if (lock.Succeeded()) {
        auto* mp = lock.GetBody().GetMotionProperties();
        if (mp != nullptr)
            mp->SetAngularDamping(damping);
    }
}

Velocity3 PhysicsWorld::getBodyPosition(BodyHandle handle) {
    if (!initialized_ || !handle.valid())
        return {0.0f, 0.0f, 0.0f};
    auto pos = physicsSystem_->GetBodyInterface().GetCenterOfMassPosition(handle.id);
    return {static_cast<float>(pos.GetX()), static_cast<float>(pos.GetY()), static_cast<float>(pos.GetZ())};
}

Rotation4 PhysicsWorld::getBodyRotation(BodyHandle handle) {
    if (!initialized_ || !handle.valid())
        return {0.0f, 0.0f, 0.0f, 1.0f};
    auto rot = physicsSystem_->GetBodyInterface().GetRotation(handle.id);
    return {rot.GetX(), rot.GetY(), rot.GetZ(), rot.GetW()};
}

BodyHandle PhysicsWorld::createDebris(const JPH::Shape* shape, float px, float py, float pz, float vx, float vy,
                                      float vz, float lifetime) {
    if (!initialized_ || shape == nullptr || lifetime <= 0.0f)
        return BodyHandle{JPH::BodyID()};

    JPH::BodyCreationSettings settings(shape, JPH::RVec3(px, py, pz), JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic,
                                       physics::kLayerDynamic);
    settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
    settings.mMassPropertiesOverride.mMass = 1.0f;

    auto& bi = physicsSystem_->GetBodyInterface();
    JPH::Body* body = bi.CreateBody(settings);
    if (body == nullptr)
        return BodyHandle{JPH::BodyID()};

    bi.AddBody(body->GetID(), JPH::EActivation::Activate);
    bi.SetLinearVelocity(body->GetID(), JPH::Vec3(vx, vy, vz));

    debris_.push_back({body->GetID(), lifetime, 0.0f});
    return BodyHandle{body->GetID()};
}

uint32_t PhysicsWorld::debrisCount() const {
    return static_cast<uint32_t>(debris_.size());
}

ConstraintHandle PhysicsWorld::createFixedConstraint(BodyHandle a, BodyHandle b) {
    if (!initialized_ || !a.valid() || !b.valid())
        return ConstraintHandle{0};

    JPH::FixedConstraintSettings settings;
    settings.mAutoDetectPoint = true;

    JPH::Constraint* constraint = nullptr;
    {
        auto& lockInterface = physicsSystem_->GetBodyLockInterface();
        JPH::BodyID bodies[] = {a.id, b.id};
        constexpr int numBodies = 2;
        JPH::BodyLockMultiWrite lock(lockInterface, bodies, numBodies);
        JPH::Body* bodyA = lock.GetBody(0);
        JPH::Body* bodyB = lock.GetBody(1);
        if (bodyA == nullptr || bodyB == nullptr)
            return ConstraintHandle{0};
        constraint = settings.Create(*bodyA, *bodyB);
    }

    physicsSystem_->AddConstraint(constraint);

    uint32_t id = nextConstraintId_++;
    constraints_[id] = constraint;
    return ConstraintHandle{id};
}

void PhysicsWorld::removeConstraint(ConstraintHandle handle) {
    if (!initialized_ || !handle.valid())
        return;

    auto it = constraints_.find(handle.id);
    if (it == constraints_.end())
        return;

    physicsSystem_->RemoveConstraint(it->second);
    constraints_.erase(it);
}

} // namespace fabric
