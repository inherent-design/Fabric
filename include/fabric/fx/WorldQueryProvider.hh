#pragma once

#include <concepts>

namespace fabric::fx {

/// A type that can sample scalar fields at world-space positions.
///
/// Game layer implements this concept (e.g. FunctionCostModifier in recurse::).
/// Engine references via concept constraint; no virtual dispatch.
template <typename T>
concept WorldQueryProvider = requires(const T& provider, float x, float y, float z, int fieldId) {
    { provider.sampleField(x, y, z, fieldId) } -> std::convertible_to<float>;
};

} // namespace fabric::fx
