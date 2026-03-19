#pragma once

#include <concepts>

namespace fabric::fx {

/// Concept for a game-layer provider that samples world-space query fields.
///
/// This keeps engine code depending on a query capability instead of
/// Recurse-specific voxel types. Future mesh-facing semantic or scalar query
/// providers can widen what is sampled without changing the engine to game
/// dependency direction.
template <typename T>
concept WorldQueryProvider = requires(const T& provider, float x, float y, float z, int fieldId) {
    { provider.sampleField(x, y, z, fieldId) } -> std::convertible_to<float>;
};

} // namespace fabric::fx
