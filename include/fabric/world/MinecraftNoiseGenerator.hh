#pragma once
#include "fabric/world/GeneratorInterface.hh"
#include <array>
#include <FastNoise/FastNoise.h>
#include <vector>

namespace fabric::world {

struct NoiseGenConfig {
    int seed = 42;
    float seaLevel = 48.0f;
    float terrainHeight = 64.0f;
    float continentalFreq = 0.003f;
    float erosionFreq = 0.008f;
    float peaksFreq = 0.015f;
    float temperatureFreq = 0.002f;
    float humidityFreq = 0.002f;
};

class MinecraftNoiseGenerator : public GeneratorInterface {
  public:
    explicit MinecraftNoiseGenerator(const NoiseGenConfig& config = {});
    void generate(simulation::SimulationGrid& grid, simulation::ChunkPos pos) override;
    std::string name() const override { return "MinecraftNoise"; }

  private:
    NoiseGenConfig config_;
    FastNoise::SmartNode<FastNoise::Simplex> continentalNode_;
    FastNoise::SmartNode<FastNoise::Simplex> erosionNode_;
    FastNoise::SmartNode<FastNoise::Simplex> peaksNode_;
    FastNoise::SmartNode<FastNoise::Simplex> temperatureNode_;
    FastNoise::SmartNode<FastNoise::Simplex> humidityNode_;

    static constexpr int kSize = 32;

    // Batch-generate a 32x32 2D noise grid at given frequency and base offsets
    void batchNoise2D(const FastNoise::SmartNode<FastNoise::Simplex>& node, float freq, float baseX, float baseZ,
                      std::array<float, kSize * kSize>& out, int seed) const;

    float computeBaseHeight(float continental, float erosion, float peaks) const;

    simulation::MaterialId selectSurfaceMaterial(float temp, float humid, float wy) const;
};

} // namespace fabric::world
