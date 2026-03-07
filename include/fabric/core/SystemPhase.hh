#pragma once

#include <cstdint>
#include <string>

namespace fabric {

/// Execution phases within each frame. Systems registered to a phase
/// run together, in dependency-resolved order within the phase.
/// Phase ordering is fixed and matches the main loop structure.
enum class SystemPhase : std::uint8_t {
    PreUpdate,   // Input polling, event dispatch, mode transitions
    FixedUpdate, // Physics, AI, simulation (runs N times per frame)
    Update,      // Per-frame logic: camera, audio, animation
    PostUpdate,  // Post-simulation cleanup, state sync
    PreRender,   // Shadow cascades, LOD selection, culling
    Render,      // Scene submission, voxel rendering, particles
    PostRender   // UI overlay, debug HUD, frame flip
};

constexpr std::size_t K_SYSTEM_PHASE_COUNT = 7;

std::string systemPhaseToString(SystemPhase phase);

} // namespace fabric
