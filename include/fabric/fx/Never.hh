#pragma once

namespace fabric::fx {

/// Uninhabited type for infallible operations.
struct Never {
    Never() = delete;
};

} // namespace fabric::fx
