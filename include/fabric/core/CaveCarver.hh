#pragma once

#include "fabric/core/FieldLayer.hh"
#include "fabric/core/Rendering.hh"
#include <cstdint>

namespace fabric {

struct CaveConfig {
    float frequency = 0.03f;
    float threshold = 0.5f;
    float worminess = 1.0f; // Cellular distance function exponent; higher = more worm-like
    float minRadius = 1.0f;
    float maxRadius = 3.0f;
    int seed = 1337;
};

// Carves connected cave systems into a density field using FastNoise2
// worm/cellular noise. Subtracts density where cave noise exceeds threshold,
// producing worm-like tunnels rather than random holes.
class CaveCarver {
  public:
    explicit CaveCarver(CaveConfig config);

    // Carve caves into the density field within the given AABB region.
    // Density values are reduced where cave noise exceeds threshold.
    void carve(FieldLayer<float>& density, const AABB& region);

    const CaveConfig& config() const;
    void setConfig(const CaveConfig& config);

  private:
    CaveConfig config_;
};

} // namespace fabric
