#pragma once

#include "recurse/world/MinecraftNoiseGenerator.hh"

namespace recurse {

/// Noise-based terrain generator. Wraps MinecraftNoiseGenerator with a
/// game-facing name for use in TerrainSystem and MainMenuSystem.
class NaturalWorldGenerator : public WorldGenerator {
  public:
    explicit NaturalWorldGenerator(const NoiseGenConfig& config) : noiseGen_(config) {}

    void generate(simulation::SimulationGrid& grid, int cx, int cy, int cz) override {
        noiseGen_.generate(grid, cx, cy, cz);
    }

    void generateToBuffer(simulation::VoxelCell* buffer, int cx, int cy, int cz) override {
        noiseGen_.generateToBuffer(buffer, cx, cy, cz);
    }

    std::string worldgenFingerprintSource() const override { return noiseGen_.worldgenFingerprintSource(); }
    uint16_t sampleMaterial(int wx, int wy, int wz) const override { return noiseGen_.sampleMaterial(wx, wy, wz); }
    int maxSurfaceHeight(int cx, int cz) const override { return noiseGen_.maxSurfaceHeight(cx, cz); }

    std::string name() const override { return "NaturalWorldGenerator"; }

  private:
    MinecraftNoiseGenerator noiseGen_;
};

} // namespace recurse
