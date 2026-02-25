#include "fabric/core/MeleeSystem.hh"

#include <algorithm>
#include <cmath>

namespace fabric {

MeleeSystem::MeleeSystem(EventDispatcher& dispatcher) : dispatcher_(dispatcher) {}

MeleeAttack MeleeSystem::createAttack(const Vector3<float, Space::World>& playerPos,
                                      const Vector3<float, Space::World>& facingDir, const MeleeConfig& config) {

    float ax = std::abs(facingDir.x);
    float ay = std::abs(facingDir.y);
    float az = std::abs(facingDir.z);

    Vec3f center = playerPos;
    Vec3f halfExtents;

    // Snap to nearest world axis for AABB alignment
    if (ax >= ay && ax >= az) {
        float sign = (facingDir.x >= 0.0f) ? 1.0f : -1.0f;
        center.x += sign * (config.reach / 2.0f);
        halfExtents = Vec3f(config.reach / 2.0f, config.height / 2.0f, config.width / 2.0f);
    } else if (ay >= ax && ay >= az) {
        float sign = (facingDir.y >= 0.0f) ? 1.0f : -1.0f;
        center.y += sign * (config.reach / 2.0f);
        halfExtents = Vec3f(config.width / 2.0f, config.reach / 2.0f, config.width / 2.0f);
    } else {
        float sign = (facingDir.z >= 0.0f) ? 1.0f : -1.0f;
        center.z += sign * (config.reach / 2.0f);
        halfExtents = Vec3f(config.width / 2.0f, config.height / 2.0f, config.reach / 2.0f);
    }

    AABB hitbox(Vec3f(center.x - halfExtents.x, center.y - halfExtents.y, center.z - halfExtents.z),
                Vec3f(center.x + halfExtents.x, center.y + halfExtents.y, center.z + halfExtents.z));

    float len = std::sqrt(facingDir.x * facingDir.x + facingDir.y * facingDir.y + facingDir.z * facingDir.z);
    Vec3f direction =
        (len > 0.0f) ? Vec3f(facingDir.x / len, facingDir.y / len, facingDir.z / len) : Vec3f(0.0f, 0.0f, 1.0f);

    return {hitbox, config.damage, config.knockback, direction};
}

std::vector<size_t> MeleeSystem::checkHits(const MeleeAttack& attack, const std::vector<AABB>& targetBounds) {

    std::vector<size_t> hits;
    for (size_t i = 0; i < targetBounds.size(); ++i) {
        if (attack.hitbox.intersects(targetBounds[i]))
            hits.push_back(i);
    }
    return hits;
}

bool MeleeSystem::canAttack(float cooldownRemaining) const {
    return cooldownRemaining <= 0.0f;
}

float MeleeSystem::updateCooldown(float remaining, float dt) const {
    return std::max(0.0f, remaining - dt);
}

void MeleeSystem::emitDamageEvent(const Vector3<float, Space::World>& targetPos, float damage,
                                  const Vector3<float, Space::World>& knockbackDir) {

    Event e("melee_damage", "MeleeSystem");
    e.setData<float>("x", targetPos.x);
    e.setData<float>("y", targetPos.y);
    e.setData<float>("z", targetPos.z);
    e.setData<float>("damage", damage);
    e.setData<float>("knockback_x", knockbackDir.x);
    e.setData<float>("knockback_y", knockbackDir.y);
    e.setData<float>("knockback_z", knockbackDir.z);
    dispatcher_.dispatchEvent(e);
}

} // namespace fabric
