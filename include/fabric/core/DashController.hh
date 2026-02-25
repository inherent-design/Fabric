#pragma once

#include "fabric/core/CharacterTypes.hh"
#include "fabric/core/Rendering.hh"

namespace fabric {

class DashController {
  public:
    struct DashResult {
        Vec3f displacement;
        bool active = false;
        bool justFinished = false;
    };

    // Start a dash (ground) or boost (air). Returns false if on cooldown.
    bool startDash(DashState& state, const CharacterConfig& config, bool isAirborne);

    // Tick active dash, return displacement for this frame
    DashResult update(DashState& state, const CharacterConfig& config, const Vec3f& dashDirection, float dt,
                      bool isAirborne);

    // Tick cooldown (call every frame regardless of dash state)
    void updateCooldown(DashState& state, float dt);
};

} // namespace fabric
