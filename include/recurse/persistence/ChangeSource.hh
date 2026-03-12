#pragma once

#include <cstdint>

namespace recurse {

/// Source classification for voxel change logging.
enum class ChangeSource : uint8_t {
    Place = 0,
    Destroy = 1,
    Physics = 2,
    Generation = 3,
};

} // namespace recurse
