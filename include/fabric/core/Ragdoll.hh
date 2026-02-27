#pragma once

#include "fabric/core/PhysicsWorld.hh"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace fabric {

struct RagdollHandle {
    uint32_t id;
    bool valid() const { return id != 0; }
};

struct RagdollConfig {
    float jointRadius = 0.05f;
    float jointMass = 1.0f;
    float blendDuration = 0.2f;
};

class Ragdoll {
  public:
    void init(PhysicsWorld* physics);
    void shutdown();

    RagdollHandle createRagdoll(int jointCount, const float* bindPoseMatrices);
    void destroyRagdoll(RagdollHandle handle);

    void activate(RagdollHandle handle);
    void deactivate(RagdollHandle handle);
    bool isActive(RagdollHandle handle) const;

    void getJointTransforms(RagdollHandle handle, float* outMatrices, int maxJoints);
    int jointCount(RagdollHandle handle) const;
    uint32_t ragdollCount() const;

  private:
    struct RagdollInstance {
        std::vector<BodyHandle> bodies;
        std::vector<ConstraintHandle> constraints;
        bool active = false;
    };

    PhysicsWorld* physics_ = nullptr;
    uint32_t nextId_ = 1;
    std::unordered_map<uint32_t, RagdollInstance> ragdolls_;
    RagdollConfig config_;
};

} // namespace fabric
