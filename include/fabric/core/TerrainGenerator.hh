#pragma once

#include "fabric/core/FieldLayer.hh"
#include "fabric/core/Rendering.hh"
#include "fabric/core/Spatial.hh"
#include <cstdint>

namespace fabric {

enum class NoiseType : uint8_t {
    Simplex,
    Perlin,
    OpenSimplex2,
    Value
};

struct TerrainConfig {
    int seed = 1337;
    float frequency = 0.01f;
    int octaves = 4;
    float lacunarity = 2.0f;
    float gain = 0.5f;
    NoiseType noiseType = NoiseType::Simplex;
};

// Fills density and essence field layers over a given AABB region using
// FastNoise2 fractal noise. Density values are normalized to [0, 1].
// Essence is derived from 3D position and density gradient.
class TerrainGenerator {
  public:
    explicit TerrainGenerator(const TerrainConfig& config);

    void generate(FieldLayer<float>& density, FieldLayer<Vector4<float, Space::World>>& essence, const AABB& region);

    const TerrainConfig& config() const;
    void setConfig(const TerrainConfig& config);

  private:
    TerrainConfig config_;
};

} // namespace fabric
