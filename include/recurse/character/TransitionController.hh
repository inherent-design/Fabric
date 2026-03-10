#pragma once

#include "fabric/render/Rendering.hh"
#include "fabric/world/ChunkedGrid.hh"
#include "recurse/character/CharacterTypes.hh"

namespace recurse {

// Engine types imported from fabric:: namespace
using fabric::ChunkedGrid;
using fabric::Vec3f;

class TransitionController {
  public:
    struct TransitionResult {
        Vec3f velocity;
        CharacterState newState;
    };

    // Grounded/Jumping -> Flying: preserve horizontal momentum, add upward impulse
    TransitionResult enterFlight(const Vec3f& currentVelocity, float upwardImpulse = 5.0f, float momentumScale = 0.8f);

    // Flying -> Falling/Grounded: check ground proximity via downward scan
    TransitionResult exitFlight(const Vec3f& currentVelocity, const Vec3f& position, const ChunkedGrid<float>& grid,
                                float groundCheckDistance = 2.0f, float densityThreshold = 0.5f);

  private:
    bool checkGroundBelow(const Vec3f& position, const ChunkedGrid<float>& grid, float distance,
                          float densityThreshold) const;
};

} // namespace recurse
