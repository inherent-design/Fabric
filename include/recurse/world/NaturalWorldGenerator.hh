#pragma once

#include "fabric/world/MinecraftNoiseGenerator.hh"
#include "recurse/world/WorldGenerator.hh"

namespace recurse {

/// Noise-based terrain generator (formerly MinecraftWorldGenerator).
/// Wraps MinecraftNoiseGenerator to conform to WorldGenerator interface.
class NaturalWorldGenerator : public WorldGenerator {
  public:
    explicit NaturalWorldGenerator(const fabric::world::NoiseGenConfig& config) : noiseGen_(config) {}

    void generate(recurse::simulation::SimulationGrid& grid, int cx, int cy, int cz) override {
        noiseGen_.generate(grid, recurse::simulation::ChunkPos{cx, cy, cz});
    }

    uint16_t sampleMaterial(int wx, int wy, int wz) const override { return noiseGen_.sampleMaterial(wx, wy, wz); }
    int maxSurfaceHeight(int cx, int cz) const override { return noiseGen_.maxSurfaceHeight(cx, cz); }

    std::string name() const override { return "NaturalWorldGenerator"; }

  private:
    fabric::world::MinecraftNoiseGenerator noiseGen_;
};

} // namespace recurse
