#include "fabric/core/TransitionController.hh"
#include <cmath>

namespace fabric {

TransitionController::TransitionResult TransitionController::enterFlight(
    const Vec3f& currentVelocity,
    float upwardImpulse,
    float momentumScale) {

    TransitionResult result;
    result.velocity = Vec3f(
        currentVelocity.x * momentumScale,
        upwardImpulse,
        currentVelocity.z * momentumScale);
    result.newState = CharacterState::Flying;
    return result;
}

TransitionController::TransitionResult TransitionController::exitFlight(
    const Vec3f& currentVelocity,
    const Vec3f& position,
    const ChunkedGrid<float>& grid,
    float groundCheckDistance,
    float densityThreshold) {

    TransitionResult result;
    result.velocity = currentVelocity;

    if (checkGroundBelow(position, grid, groundCheckDistance, densityThreshold)) {
        result.newState = CharacterState::Grounded;
        result.velocity.y = 0.0f;
    } else {
        result.newState = CharacterState::Falling;
    }

    return result;
}

bool TransitionController::checkGroundBelow(
    const Vec3f& position,
    const ChunkedGrid<float>& grid,
    float distance,
    float densityThreshold) const {

    int x = static_cast<int>(std::floor(position.x));
    int z = static_cast<int>(std::floor(position.z));
    int startY = static_cast<int>(std::floor(position.y)) - 1;
    int endY = static_cast<int>(std::floor(position.y - distance));

    for (int y = startY; y >= endY; --y) {
        if (grid.get(x, y, z) >= densityThreshold) {
            return true;
        }
    }
    return false;
}

} // namespace fabric
