#pragma once

#include "recurse/world/WorldGenerator.hh"

namespace recurse {

/// Flat world: stone below y=groundLevel, air above.
/// Used for simulation testing and initial VP0+ demo.
class FlatWorldGenerator : public WorldGenerator {
  public:
    explicit FlatWorldGenerator(int groundLevel = 32);

    void generate(recurse::simulation::SimulationGrid& grid, int cx, int cy, int cz) override;
    uint16_t sampleMaterial(int wx, int wy, int wz) const override;
    int maxSurfaceHeight(int cx, int cz) const override;
    std::string worldgenFingerprintSource() const override;
    std::string name() const override { return "FlatWorldGenerator"; }

  private:
    int groundLevel_;
};

/// Layered world: stone base, sand layer on top, air above.
/// Tests material stacking and sand-over-stone behavior.
class LayeredWorldGenerator : public WorldGenerator {
  public:
    LayeredWorldGenerator(int stoneLevel = 28, int sandDepth = 4);

    void generate(recurse::simulation::SimulationGrid& grid, int cx, int cy, int cz) override;
    uint16_t sampleMaterial(int wx, int wy, int wz) const override;
    int maxSurfaceHeight(int cx, int cz) const override;
    std::string worldgenFingerprintSource() const override;
    std::string name() const override { return "LayeredWorldGenerator"; }

  private:
    int stoneLevel_;
    int sandDepth_;
};

} // namespace recurse
