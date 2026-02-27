#include "fabric/core/Ragdoll.hh"
#include "fabric/core/Log.hh"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>

#include <cstring>
#include <vector>

namespace fabric {

void Ragdoll::init(PhysicsWorld* physics) {
    physics_ = physics;
    FABRIC_LOG_INFO("Ragdoll system initialized");
}

void Ragdoll::shutdown() {
    std::vector<uint32_t> keys;
    keys.reserve(ragdolls_.size());
    for (auto& [id, inst] : ragdolls_)
        keys.push_back(id);
    for (auto id : keys)
        destroyRagdoll(RagdollHandle{id});
    ragdolls_.clear();
}

RagdollHandle Ragdoll::createRagdoll(int jointCount, const float* bindPoseMatrices) {
    if (physics_ == nullptr || jointCount <= 0 || bindPoseMatrices == nullptr)
        return RagdollHandle{0};

    RagdollInstance inst;
    float halfHeight = config_.jointRadius * 2.0f;
    JPH::Ref<JPH::CapsuleShape> shape = new JPH::CapsuleShape(halfHeight, config_.jointRadius);

    for (int i = 0; i < jointCount; ++i) {
        const float* m = bindPoseMatrices + i * 16;
        // Column-major 4x4: translation at indices 12, 13, 14
        float px = m[12];
        float py = m[13];
        float pz = m[14];

        BodyHandle body = physics_->createDynamicBody(shape.GetPtr(), px, py, pz, config_.jointMass);
        inst.bodies.push_back(body);
    }

    // Chain sequential constraints: joint[0]-joint[1], joint[1]-joint[2], etc.
    for (int i = 1; i < jointCount; ++i) {
        ConstraintHandle ch = physics_->createFixedConstraint(inst.bodies[i - 1], inst.bodies[i]);
        inst.constraints.push_back(ch);
    }

    uint32_t id = nextId_++;
    ragdolls_[id] = std::move(inst);
    return RagdollHandle{id};
}

void Ragdoll::destroyRagdoll(RagdollHandle handle) {
    if (physics_ == nullptr || !handle.valid())
        return;

    auto it = ragdolls_.find(handle.id);
    if (it == ragdolls_.end())
        return;

    auto& inst = it->second;
    for (auto& ch : inst.constraints)
        physics_->removeConstraint(ch);
    for (auto& bh : inst.bodies)
        physics_->removeBody(bh);

    ragdolls_.erase(it);
}

void Ragdoll::activate(RagdollHandle handle) {
    auto it = ragdolls_.find(handle.id);
    if (it == ragdolls_.end())
        return;

    it->second.active = true;
    for (auto& bh : it->second.bodies)
        physics_->applyImpulse(bh, 0.0f, 0.001f, 0.0f);
}

void Ragdoll::deactivate(RagdollHandle handle) {
    auto it = ragdolls_.find(handle.id);
    if (it == ragdolls_.end())
        return;

    it->second.active = false;
    for (auto& bh : it->second.bodies)
        physics_->setLinearVelocity(bh, 0.0f, 0.0f, 0.0f);
}

bool Ragdoll::isActive(RagdollHandle handle) const {
    auto it = ragdolls_.find(handle.id);
    if (it == ragdolls_.end())
        return false;
    return it->second.active;
}

void Ragdoll::getJointTransforms(RagdollHandle handle, float* outMatrices, int maxJoints) {
    auto it = ragdolls_.find(handle.id);
    if (it == ragdolls_.end() || outMatrices == nullptr)
        return;

    auto& inst = it->second;
    int count = static_cast<int>(inst.bodies.size());
    if (maxJoints < count)
        count = maxJoints;

    for (int i = 0; i < count; ++i) {
        Velocity3 pos = physics_->getBodyPosition(inst.bodies[i]);
        Rotation4 rot = physics_->getBodyRotation(inst.bodies[i]);

        float* m = outMatrices + i * 16;

        // Quaternion to 3x3 rotation (column-major layout)
        float xx = rot.x * rot.x;
        float yy = rot.y * rot.y;
        float zz = rot.z * rot.z;
        float xy = rot.x * rot.y;
        float xz = rot.x * rot.z;
        float yz = rot.y * rot.z;
        float wx = rot.w * rot.x;
        float wy = rot.w * rot.y;
        float wz = rot.w * rot.z;

        m[0] = 1.0f - 2.0f * (yy + zz);
        m[1] = 2.0f * (xy + wz);
        m[2] = 2.0f * (xz - wy);
        m[3] = 0.0f;

        m[4] = 2.0f * (xy - wz);
        m[5] = 1.0f - 2.0f * (xx + zz);
        m[6] = 2.0f * (yz + wx);
        m[7] = 0.0f;

        m[8] = 2.0f * (xz + wy);
        m[9] = 2.0f * (yz - wx);
        m[10] = 1.0f - 2.0f * (xx + yy);
        m[11] = 0.0f;

        m[12] = pos.x;
        m[13] = pos.y;
        m[14] = pos.z;
        m[15] = 1.0f;
    }
}

int Ragdoll::jointCount(RagdollHandle handle) const {
    auto it = ragdolls_.find(handle.id);
    if (it == ragdolls_.end())
        return 0;
    return static_cast<int>(it->second.bodies.size());
}

uint32_t Ragdoll::ragdollCount() const {
    return static_cast<uint32_t>(ragdolls_.size());
}

} // namespace fabric
