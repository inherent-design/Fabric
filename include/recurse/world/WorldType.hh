#pragma once

namespace recurse {

enum class WorldType {
    Flat,   // FlatWorldGenerator: stone below y=groundLevel, air above
    Natural // NaturalWorldGenerator: noise-based terrain with caves, layers
};

} // namespace recurse
