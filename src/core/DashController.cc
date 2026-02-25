#include "fabric/core/DashController.hh"
#include <algorithm>

namespace fabric {

bool DashController::startDash(DashState& state, const CharacterConfig& config, bool isAirborne) {
    if (state.cooldownRemaining > 0.0f) {
        return false;
    }

    state.active = true;
    state.durationRemaining = config.dashDuration;
    state.cooldownRemaining = isAirborne ? config.boostCooldown : config.dashCooldown;
    return true;
}

DashController::DashResult DashController::update(DashState& state, const CharacterConfig& config,
                                                  const Vec3f& dashDirection, float dt, bool isAirborne) {

    DashResult result;

    if (!state.active) {
        return result;
    }

    float speed = isAirborne ? config.boostSpeed : config.dashSpeed;
    result.displacement = dashDirection * (speed * dt);
    result.active = true;

    state.durationRemaining -= dt;
    if (state.durationRemaining <= 0.0f) {
        state.durationRemaining = 0.0f;
        state.active = false;
        result.active = false;
        result.justFinished = true;
    }

    return result;
}

void DashController::updateCooldown(DashState& state, float dt) {
    if (state.cooldownRemaining > 0.0f) {
        state.cooldownRemaining = std::max(0.0f, state.cooldownRemaining - dt);
    }
}

} // namespace fabric
