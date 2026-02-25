#pragma once

#include "fabric/core/Event.hh"
#include "fabric/core/Rendering.hh"
#include "fabric/core/Spatial.hh"

#include <cstddef>
#include <vector>

namespace fabric {

struct MeleeConfig {
    float reach = 3.0f;
    float width = 2.0f;
    float height = 2.0f;
    float cooldown = 0.5f;
    float damage = 10.0f;
    float knockback = 5.0f;
};

struct MeleeAttack {
    AABB hitbox;
    float damage;
    float knockback;
    Vector3<float, Space::World> direction;
};

class MeleeSystem {
  public:
    MeleeSystem(EventDispatcher& dispatcher);

    // Generate attack hitbox from player position + facing direction
    // Aligns hitbox to nearest world axis (Sprint 5b simplification)
    MeleeAttack createAttack(
        const Vector3<float, Space::World>& playerPos,
        const Vector3<float, Space::World>& facingDir,
        const MeleeConfig& config);

    // Check if attack hitbox overlaps any target AABBs. Returns indices of hit targets.
    std::vector<size_t> checkHits(
        const MeleeAttack& attack,
        const std::vector<AABB>& targetBounds);

    bool canAttack(float cooldownRemaining) const;
    float updateCooldown(float remaining, float dt) const;

    void emitDamageEvent(
        const Vector3<float, Space::World>& targetPos,
        float damage,
        const Vector3<float, Space::World>& knockbackDir);

  private:
    EventDispatcher& dispatcher_;
};

} // namespace fabric
